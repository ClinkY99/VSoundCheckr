//
// Created by cline on 2025-03-26.
//

#include "SaveFileDB.h"

#include <iostream>

#define PROJECT_PAGE_SIZE 65536

#define str(a) #a
#define xstr(a) str(a)

static const char *Config =
   "PRAGMA main.busy_timeout = 5000;"
   "PRAGMA main.locking_mode = SHARED;"
   "PRAGMA main.synchronous = NORMAL;"
   "PRAGMA main.journal_mode = OFF;";

static const char* PageSizeConfig =
   "PRAGMA main.page_size = " xstr(PROJECT_PAGE_SIZE) ";"
   "VACUUM;";

bool SaveFileDB::newSave(FilePath save) {
    if (open(save, true)) {
        std::cout<<"error opening DB"<<std::endl;
        return false;
    }

    initSaveDB();
    return true;
}

int SaveFileDB::open(FilePath save, bool newSave) {
    assert(mDB == nullptr);
    mSavePath = save;

    if (newSave) {
        std::remove(save.c_str());
        std::remove(save.c_str()+"-shm");
        std::remove(save.c_str()+"-wal");
    }

    int err =  sqlite3_open(save, &mDB);

    if (err != SQLITE_OK) {
        assert(false);
        return -1;
    }

    if (newSave) {
        err = sqlite3_exec(mDB, PageSizeConfig, nullptr, nullptr, nullptr);
    }
    err = sqlite3_exec(mDB, Config, nullptr, nullptr, nullptr);
    if (err != SQLITE_OK) {
        std::cerr<<sqlite3_errmsg(mDB)<<std::endl;
    }


    return err;
}

void SaveFileDB::close() {
    assert(mDB != nullptr);

    sqlite3_close(mDB);
    mDB = nullptr;
}


void SaveFileDB::initSaveDB() {
    const char * sql = "CREATE TABLE IF NOT EXISTS sampleBlocks ( "
                        "blockID INTEGER PRIMARY KEY, "
                        "sampleformat INTEGER, "
                        "summin REAL, "
                        "summax REAL, "
                        "sumrms REAL, "
                        "samples BLOB,"
                        "summary256 BLOB,"
                        "summary64k BLOB);";

    sqlite3_exec(DB(), sql, nullptr, nullptr, nullptr);

    sql = "CREATE TABLE IF NOT EXISTS tracks ("
          "trackNum INTEGER PRIMARY KEY,"
          "trackType INTEGER,"
          "firstChannelIn INTEGER,"
          "firstChannelOut INTEGER,"
          "sampleRate REAL,"
          "blocks BLOB);";

    sqlite3_exec(DB(), sql, nullptr, nullptr, nullptr);

    sql = "CREATE TABLE IF NOT EXISTS settings ("
          "_ INTEGER PRIMARY KEY,"
          "hostAPI INTEGER,"
          "inDev INTEGER,"
          "outDEV INTEGER,"
          "sRate REAL);";

    sqlite3_exec(DB(), sql, nullptr, nullptr, nullptr);

    //SNAPSHOT TABLE
    sql = "CREATE TABLE IF NOT EXISTS snapshots ("
          "snapshotNum INTEGER PRIMARY KEY,"
          "time REAL,"
          "controller INTEGER,"
          "change INTEGER,"
          "name TEXT);";

    sqlite3_exec(DB(), sql, nullptr, nullptr, nullptr);
}

sqlite3_stmt *SaveFileDB::Prepare(const char *sql) {
    sqlite3_stmt* stmt;
    auto err = sqlite3_prepare_v3(DB(), sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt, 0);

    if (err != SQLITE_OK) {
        //FAILED TO PREPARE STATMENT
        std::cout<< "Err Code: "<< sqlite3_errmsg(DB()) << std::endl;
        wxASSERT(false);
    }

    return stmt;
}


