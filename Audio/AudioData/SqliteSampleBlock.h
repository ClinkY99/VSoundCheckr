//
// Created by cline on 2025-03-09.
//

#ifndef SQLITESAMPLEBLOCK_H
#define SQLITESAMPLEBLOCK_H
#include <map>
#include <sqlite3.h>

#include "SampleBlock.h"

class SqliteSampleBlockFactory;

class SqliteSampleBlock {

public:
    SqliteSampleBlock();

};


class SqliteSampleBlockFactory
    : public SampleBlockFactory
    , public std::enable_shared_from_this<SqliteSampleBlockFactory> {

    using allBlocksMap = std::map<SampleBlockID, std::weak_ptr<SqliteSampleBlock>>;
    sqlite3* m_db;

public:
    SqliteSampleBlockFactory();

    sqlite3* DB();

protected:
    SampleBlockPtr DoCreate(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) override;
    SampleBlockPtr DoCreateSilent(size_t nSamples, SampleFormat srcFormat) override;
    SampleBlockPtr DoCreateID(SampleFormat srcFormat, SampleBlockID srcBlockID) override;



};



#endif //SQLITESAMPLEBLOCK_H
