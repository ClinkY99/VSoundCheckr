/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef SQLITESAMPLEBLOCK_H
#define SQLITESAMPLEBLOCK_H
#include <map>
#include <sqlite3.h>

#include "SampleBlock.h"
#include "../../MemoryManagement/MemoryTypes.h"
#include "../../Saving/DBConnection.h"
#include "../IO/AudioIO.h"

class SqliteSampleBlockFactory;

//NumBytes for 256 and 64k summaries
using Sizes = std::pair<size_t, size_t>;

class SqliteSampleBlock
    : public SampleBlock {

    friend SqliteSampleBlockFactory;

    std::weak_ptr<std::vector<float>> mCache;
    std::mutex mCacheMutex;

    const std::shared_ptr<SqliteSampleBlockFactory> mFactory;

    bool mLocked {false};
    bool mValid {true};

    SampleBlockID mBlockID{0};

    //Samples
    ArrayOf<char> mSamples;
    SampleFormat mSampleFormat;
    size_t mSampleCount;
    size_t mSampleBytes;

    //Sample Data
    ArrayOf<char> mSummary256;
    ArrayOf<char> mSummary64k;
    double mSumMin;
    double mSumMax;
    double mSumRMS;

    enum {
        fields = 3,
        bytesPerFrame = fields*sizeof(float)
    };

public:
    SqliteSampleBlock(const std::shared_ptr<SqliteSampleBlockFactory> &factory);
    ~SqliteSampleBlock();

    void lock() override;
    bool isSilent() override;
    SampleBlockID getBlockID() override {return mBlockID;}
    SampleFormat getSampleFormat() override {return mSampleFormat;}
    size_t getSampleCount() override {return mSampleCount;}


    bool GetSummary64k(float *dest, size_t offset, size_t nFrames) override;
    bool GetSummary256(float *dest, size_t offset, size_t nFrames) override;
    double getSumMax() const {return mSumMax;}
    double getSumMin() const {return mSumMin;}
    double getSumRMS() const {return mSumRMS;}

    BlockSampleView GetFloatSampleView() override;

    void SetSamples(constSamplePtr src, SampleFormat srcFormat, size_t numSamples);
    void Commit(Sizes sizes);
    void Delete();

private:
    size_t DoGetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) override;
    MaxMinRMS DoGetMaxMinRMS(size_t start, size_t len) override;
    MaxMinRMS DoGetMaxMinRMS() override;

    bool GetSummaries(float* dest, size_t offset, size_t nFrames, DBConnection::statementID id, const char* sql);
    bool CalcSummaries(Sizes sizes);

    Sizes SetSizes(size_t numSamples, SampleFormat srcFormat);

    void load(SampleBlockID id);

    //GetDBStuff
    DBConnection* Conn();
    sqlite3* DB();

    size_t GetBlob(sqlite3_stmt* stmt, void* dest, SampleFormat destFormat, SampleFormat srcFormat, size_t srcOffset, size_t srcBytes);
};


class SqliteSampleBlockFactory
    : public SampleBlockFactory
    , public std::enable_shared_from_this<SqliteSampleBlockFactory> {

    using allBlocksMap = std::map<SampleBlockID, std::weak_ptr<SqliteSampleBlock>>;

    allBlocksMap mAllBlocks;

    static std::map<SampleBlockID, std::shared_ptr<SqliteSampleBlock>> sSilentBlocks;

    std::shared_ptr<DBConnection> mDB;


public:
    SqliteSampleBlockFactory();
    ~SqliteSampleBlockFactory() override;

    DBConnection* DBConn();

protected:
    SampleBlockPtr DoCreate(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) override;
    SampleBlockPtr DoCreateSilent(size_t nSamples, SampleFormat srcFormat) override;
    SampleBlockPtr DoCreateID(SampleFormat srcFormat, SampleBlockID srcBlockID) override;

};



#endif //SQLITESAMPLEBLOCK_H
