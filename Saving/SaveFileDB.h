//
// Created by cline on 2025-03-26.
//

#ifndef SAVEFILEDB_H
#define SAVEFILEDB_H
#include "DBConnection.h"


class SaveFileDB{

    FilePath mSavePath;

    sqlite3* mDB;

public:
    bool newSave(FilePath save);
    int open(FilePath save, bool newSave = false);
    void close();

    sqlite3_stmt* Prepare(const char* sql);

    FilePath GetSavePath() {return mSavePath;};

    sqlite3* DB(){return mDB;}

private:
    void initSaveDB();
};

using SaveFile = std::shared_ptr<SaveFileDB>;

struct SaveBase {
    virtual ~SaveBase() = default;

    SaveFile mSaveConn;

    virtual void save() = 0;
    virtual void newShow() = 0;
    virtual void load(int id) = 0;
};



#endif //SAVEFILEDB_H
