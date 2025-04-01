//
// Created by cline on 2025-03-09.
//

#ifndef DBCONNECTION_H
#define DBCONNECTION_H
#include <condition_variable>
#include <map>
#include <sqlite3.h>
#include <thread>
#include <utility>

#include "../project.h"


class DBConnection {
public:
    enum statementID {
        GetSamples,
        GetSummary256,
        GetSummary64k,
        LoadSampleBlock,
        InsertSampleBlock,
        DeleteSampleBlock,
        GetSampleBlockSize,
        GetAllSampleBlocksSize
    };
private:
    using StatementIndex = std::pair<statementID, std::thread::id>;
    sqlite3* mDB;
    sqlite3* mCheckpointDB;

    FilePath mPath;

    //thread stuff
    std::thread mCheckpointThread;
    std::mutex mCheckpointMutex;
    std::condition_variable mCheckpointCV;

    std::atomic_bool mCheckpointStop {false};
    std::atomic_bool mCheckpointActive {false};
    std::atomic_bool mCheckpointPending {false};

    std::mutex mStatementMutex;
    std::map<StatementIndex, sqlite3_stmt*> mStatements;

public:
    DBConnection();
    ~DBConnection();
    sqlite3* DB();

    sqlite3_stmt* Prepare(statementID id, const char* sql);

    int open(const FilePath fileName, bool newFile = true);
    bool close();

    //sqlite mode setting
    int ModeConfig(sqlite3* db, const char* schema, const char* config);
    int FastMode(const char* schema = "main");
    int SafeMode(const char* schema = "main");
    int setPageSize(const char* schema = "main");

    FilePath getPath() const {return mPath;}

private:
    //Checkpoint Thread Stuff
    void checkpointThread(sqlite3* db, FilePath fileName);
    static int checkpointHook(void * data, sqlite3 * db, const char * schema, int pages);

    int openStepByStep(const FilePath fileName, bool newFile);







};



#endif //DBCONNECTION_H
