//
// Created by cline on 2025-03-09.
//

#include "DBConnection.h"

#define PROJECT_PAGE_SIZE 65536

#define str(a) #a
#define xstr(a) str(a)

static const char* PageSizeConfig =
   "PRAGMA <schema>.page_size = " xstr(PROJECT_PAGE_SIZE) ";"
   "VACUUM;";

// Configuration to provide "safe" connections
static const char* SafeConfig =
   "PRAGMA <schema>.busy_timeout = 5000;"
   "PRAGMA <schema>.locking_mode = SHARED;"
   "PRAGMA <schema>.synchronous = NORMAL;"
   "PRAGMA <schema>.journal_mode = WAL;"
   "PRAGMA <schema>.wal_autocheckpoint = 0;";

// Configuration to provide "Fast" connections
static const char *FastConfig =
   "PRAGMA <schema>.busy_timeout = 5000;"
   "PRAGMA <schema>.locking_mode = SHARED;"
   "PRAGMA <schema>.synchronous = OFF;"
   "PRAGMA <schema>.journal_mode = OFF;";

DBConnection::DBConnection() {
   mDB = nullptr;
   mCheckpointDB = nullptr;
}
DBConnection::~DBConnection() {
   assert(mDB == nullptr);
   if (mDB != nullptr) {
      //LOG, DATABASE NOT CLOSED
   }
}

int DBConnection::open(const FilePath fileName) {

   assert(mDB == nullptr);
   int err;

   mCheckpointActive = false;
   mCheckpointPending = false;
   mCheckpointStop = false;

   err = openStepByStep(fileName);

   //if error occurs clear databases
   if (err != SQLITE_OK) {
      if (mDB) {
         sqlite3_close(mDB);
         mDB = nullptr;
      }
      if (mCheckpointDB) {
         sqlite3_close(mCheckpointDB);
         mCheckpointDB = nullptr;
      }
   }

   return err;
}

int DBConnection::openStepByStep(const FilePath fileName) {
   const char* filePath = fileName.ToUTF8();
   bool success =true;

   int err = sqlite3_open(filePath, &mDB);

   if (err != SQLITE_OK) {
      //ERROR CONNECTING TO MAIN DB
      throw;
   }

   err = setPageSize();

   if (err != SQLITE_OK) {
      //ERR SETTING PAGE SIZE
      throw;
   }

   err = SafeMode();

   if (err != SQLITE_OK) {
      //FAILED TO SET TO SAFE MODE
      throw;
   }

   err = sqlite3_open(filePath, &mCheckpointDB);

   if (err != SQLITE_OK) {
      //FAILED TO OPEN CHECKPOINT DB
      throw;
   }

   err = ModeConfig(mCheckpointDB, "main", SafeConfig);

   if (err != SQLITE_OK) {
      //FAILED TO SET CHECKPOINT CONFIG
   }

   //open checkpoint thread and bind checkpoint hook
   auto db = mCheckpointDB;
   mCheckpointThread = std::thread([this, db, fileName] {checkpointThread(db,fileName);});

   sqlite3_wal_hook(mDB, checkpointHook,this);

   return err;
}

sqlite3_stmt* DBConnection::Prepare(statementID id, const char *sql) {
   std::lock_guard<std::mutex> guard(mStatementMutex);

   int err;

   //no 2 threads can have the same index
   StatementIndex ndx(id, std::this_thread::get_id());

   //If the stmt already exists return existing statment
   auto iter = mStatements.find(ndx);
   if (iter != mStatements.end()) {
      return iter->second;
   }
   sqlite3_stmt* stmt;
   err = sqlite3_prepare_v3(mDB, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, 0);

   if (err != SQLITE_OK) {
      //FAILED TO PREPARE STATMENT
      throw;
   }

   //save the statement
   mStatements.insert({ndx, stmt});

   return stmt;
}

bool DBConnection::close() {
   assert(mDB != nullptr);

   int err;

   //protection
   if (mDB == nullptr) {
      return true;
   }

   //disconnect checkpoint hook
   sqlite3_wal_hook(mDB, nullptr, nullptr);

   //If waiting for checkpoints display message to user
   if (mCheckpointActive || mCheckpointPending) {
      //DISPLAY MESSAGE

      while (mCheckpointActive||mCheckpointPending) {
         using namespace std::chrono;
         std::this_thread::sleep_for(50ms);
      }
   }

   {
      std::lock_guard<std::mutex> guard(mCheckpointMutex);
      mCheckpointStop = true;
      mCheckpointCV.notify_one();
   }

   //wait for thread to complete
   if (mCheckpointThread.joinable()) {
      mCheckpointThread.join();
   }

   //clear Statments
   {
      std::lock_guard<std::mutex> guard(mStatementMutex);
      for (auto& statement: mStatements) {
         int err = sqlite3_finalize(statement.second);
         if (err != SQLITE_OK) {
            //FAILED TO FINALIZE STATEMENT
            throw;
         }
      }

      mStatements.clear();
   }

   //close the DB connections
   sqlite3_close(mDB);
   sqlite3_close(mCheckpointDB);

   mDB = nullptr;
   mCheckpointActive = false;

   return true;
}





sqlite3 *DBConnection::DB() {
   return mDB;
}

int DBConnection::ModeConfig(sqlite3 *db, const char *schema, const char *config) {
   int err;

   //Setup config string
   wxString sql = config;
   sql.Replace(wxT("<schema>"), schema);

   err = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);

   if (err != SQLITE_OK) {
      //FAILED TO CONFIG
      throw;
   }

   return err;
}

int DBConnection::FastMode(const char *schema) {
   return ModeConfig(mDB, schema, FastConfig);
}

int DBConnection::SafeMode(const char *schema) {
   return ModeConfig(mDB, schema, SafeConfig);
}

int DBConnection::setPageSize(const char *schema) {
   //IN FUTURE SHOULD PERFORM CHECK TO ENSURE EMPTY

   return ModeConfig(mDB, schema, PageSizeConfig);
}


//CHECKPOINT THREAD STUFF

int DBConnection::checkpointHook(void *data, sqlite3 *db, const char *schema, int pages) {
   DBConnection* that = static_cast<DBConnection*>(data);

   std::lock_guard<std::mutex> guard(that->mStatementMutex);
   that->mCheckpointPending = true;
   that->mCheckpointCV.notify_one();

   return SQLITE_OK;
}

void DBConnection::checkpointThread(sqlite3 *db, FilePath fileName) {
   int err = SQLITE_OK;
   bool giveup = false;

   while (true) {

      {
         std::unique_lock<std::mutex> lock(mCheckpointMutex);
         mCheckpointCV.wait(lock, [&] {
            return mCheckpointPending||mCheckpointStop;
         });

         //We are being told to exit thread
         if (mCheckpointStop) {
            break;
         }

         mCheckpointActive = true;
         mCheckpointPending =false;
      }

      {
         using namespace std::chrono;

         do {
            err = sqlite3_wal_checkpoint_v2(mCheckpointDB, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
         } while (err == SQLITE_BUSY && (std::this_thread::sleep_for(1ms),true));

      }

      if (err != SQLITE_OK) {
         //NEEDS MORE ERROR HANDLING
         //MOST LIKELY TO OCCUR BECAUSE OF DISK FULL EXCEPTIONS
         //DISPLAY MESSAGE
         giveup = true;
         //END AUDIO THREAD
         //EXIT AUDIO
         throw;
      }
   }
}








