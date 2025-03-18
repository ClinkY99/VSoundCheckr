//
// Created by cline on 2025-03-09.
//

#include "SqliteSampleBlock.h"

#include <cmath>
#include <cfloat>
#include <iostream>

#include "../Dither.h"
#include "../SampleCount.h"
#include "../IO/AudioIO.h"


std::map<SampleBlockID, std::shared_ptr<SqliteSampleBlock>> SqliteSampleBlockFactory::sSilentBlocks;

//FACTORY FUNCTIONS
SqliteSampleBlockFactory::SqliteSampleBlockFactory() {
    mDB = AudioIOBase::sAudioDB;
}
SqliteSampleBlockFactory::~SqliteSampleBlockFactory() =default;

DBConnection *SqliteSampleBlockFactory::DBConn() {
    return mDB.get();
}

SampleBlockPtr SqliteSampleBlockFactory::DoCreate(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) {
    auto sb = std::make_shared<SqliteSampleBlock>(shared_from_this());

    sb->SetSamples(src, srcFormat, numSamples);

    if (sb.get()) {
        mAllBlocks[sb->getBlockID()] = sb;
    } else {
        throw;
    }

    return sb;
}

SampleBlockPtr SqliteSampleBlockFactory::DoCreateID(SampleFormat srcFormat, SampleBlockID srcBlockID) {
    if (srcBlockID <=0) {
        return DoCreateSilent(-srcBlockID, srcFormat);
    }

    auto &wp = mAllBlocks[srcBlockID];
    if (auto block = wp.lock()) {
        return block;
    }

    auto sb = std::make_shared<SqliteSampleBlock>(shared_from_this());

    wp = sb;

    sb->mSampleFormat = srcFormat;

    sb->load(srcBlockID);

    return sb;
}
SampleBlockPtr SqliteSampleBlockFactory::DoCreateSilent(size_t nSamples, SampleFormat srcFormat) {
    SampleBlockID id = -nSamples;
    auto &sb = sSilentBlocks[id];
    if (!sb) {
        sb = std::make_shared<SqliteSampleBlock>(shared_from_this());

        sb->mBlockID = id;

        sb->SetSizes(nSamples, floatSample);
        sb->mValid = true;
    }
    return sb;
}

//SAMPLE BLOCK FUNCTIONS
SqliteSampleBlock::SqliteSampleBlock(const std::shared_ptr<SqliteSampleBlockFactory>& factory)
    : mFactory(factory) {

    mSampleFormat = floatSample;
    mSampleCount = 0;
    mSampleBytes = 0;

    mSumMax = 0;
    mSumMin = 0;
    mSumRMS = 0;
}

SqliteSampleBlock::~SqliteSampleBlock() {
    mSamples.release();
    mSummary64k.release();
    mSummary256.release();
}

void SqliteSampleBlock::SetSamples(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) {
    auto sizes = SetSizes(numSamples, srcFormat);
    mSamples.Reinit(mSampleBytes);

    memcpy(mSamples.get(), src, numSamples*SAMPLE_SIZE(srcFormat));

    CalcSummaries(sizes);

    Commit(sizes);
}

void SqliteSampleBlock::load(SampleBlockID id) {
    mValid = false;
    assert(id >0);

    auto* stmt = Conn()->Prepare(DBConnection::LoadSampleBlock,
        "SELECT sampleformat, summin, summax, sumrms, length(samples)"
        "    FROM sampleBlocks WHERE blockID = ?1;");

    //Bind blockID
    if (sqlite3_bind_int(stmt, 1, id)) {
        //BINDING FAIlED (replace with log)
        throw;
    }

    //Complete Statement
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        //EXECUTE FAIlED (replace with log)
        throw;
    }

    mBlockID = id;
    mSampleFormat = (SampleFormat) sqlite3_column_int(stmt, 0);
    mSumMin = sqlite3_column_double(stmt, 1);
    mSumMax = sqlite3_column_double(stmt, 2);
    mSumRMS = sqlite3_column_double(stmt, 3);
    mSampleBytes = sqlite3_column_int(stmt, 4);
    mSampleCount = mSampleBytes/SAMPLE_SIZE(mSampleFormat);

    //Finish statment and reset for future use
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    mValid = true;
}

void SqliteSampleBlock::Commit(Sizes sizes) {
    const auto summary256Bytes = sizes.first;
    const auto summary64kBytes = sizes.second;

    auto* stmt = Conn()->Prepare(DBConnection::InsertSampleBlock,
        "INSERT INTO sampleBlocks (sampleformat, summin, summax, sumrms,"
        "                              samples, summary256, summary64k)"
        "                              VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7);");


    //INPUT VALUES
    if (sqlite3_bind_int(stmt, 1, static_cast<int>(mSampleFormat)) ||
        sqlite3_bind_double(stmt, 2, mSumMin) ||
        sqlite3_bind_double(stmt, 3, mSumMax) ||
        sqlite3_bind_double(stmt, 4, mSumRMS) ||
        sqlite3_bind_blob(stmt, 5, mSamples.get(), mSampleBytes, SQLITE_STATIC) ||
        sqlite3_bind_blob(stmt, 6, mSummary256.get(), summary256Bytes, SQLITE_STATIC) ||
        sqlite3_bind_blob(stmt, 7, mSummary64k.get(), summary64kBytes, SQLITE_STATIC))
        {
        //BINDING FAIlED (replace with log)
        throw;
    }
    //Perform step
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        //STEP FAILED (replace with log)
        throw;
    }

    mBlockID = sqlite3_last_insert_rowid(DB());

    mSamples.reset();
    mSummary64k.reset();
    mSummary256.reset();
    {
        std::lock_guard<std::mutex> lock(mCacheMutex);
        mCache.reset();
    }

    //Clear bindings and reset stmt for future use
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    mValid = true;
}

void SqliteSampleBlock::Delete() {
    auto* stmt = Conn()->Prepare(DBConnection::DeleteSampleBlock,
        "DELETE FROM sampleblocks WHERE blockID = ?1;");

    //Bind Block ID
    if (sqlite3_bind_int(stmt, 1, mBlockID)) {
        //BINDING FAIlED (replace with log)
        throw;
    }

    //execute stmt
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        //EXECUTE FAIlED (replace with log)
        throw;
    }

    //Reset Stmt for future use
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);
}

bool SqliteSampleBlock::CalcSummaries(Sizes sizes) {
    const auto summary256Bytes = sizes.first;
    const auto summary64kBytes = sizes.second;

    float* samples;

    if (mSampleFormat == floatSample) {
        samples = (float* )mSamples.get();
    } else {
        Floats sampleBuffer;
        sampleBuffer.Reinit(mSampleBytes);
        SamplesToFloat(mSamples.get(), mSampleFormat, sampleBuffer.get(), mSampleBytes);
        samples = sampleBuffer.get();
    }

    mSummary256.Reinit(summary256Bytes);
    mSummary64k.Reinit(summary64kBytes);

    float* summary256 = (float*) mSummary256.get();
    float* summary64k = (float*) mSummary64k.get();

    float max;
    float min;
    float sumSq;
    double totalSquares = 0;
    double fractions = 0;

    int sumLen = (mSampleCount+255)/256;
    int summaries = 256;

    for (int i = 0; i < sumLen; ++i) {
        min = samples[i*256];
        max = samples[i*256];
        sumSq = min*min;

        int jcount = 256;
        if (jcount > mSampleCount-i*256) {
            jcount = mSampleCount-i*256;
            fractions = 1.0-jcount/256;
        }

        for (int j =i; j<jcount; j++) {
            float f1 = samples[i*256+j];
            sumSq += f1*f1;

            min = std::min(min, f1);
            max = std::max(max, f1);
        }

        totalSquares += sumSq;

        //Save min,max,and RMS
        summary256[i*fields] = min;
        summary256[i*fields+1] = max;
        summary256[i*fields+2] = sqrt(sumSq/jcount);
    }

    //We are missing some data so fill remaining summary frames with non harmful data
    for (int i = sumLen; i<summary256Bytes/bytesPerFrame; i++) {
        summaries--;

        summary256[i*fields] = FLT_MAX; //min
        summary256[i*fields+1] = -FLT_MAX; //max
        summary256[i*fields+2] = 0.0f;
    }

    //Calc RMS
    mSumRMS = sqrt(totalSquares/mSampleCount);

    //Recalc the 64k Size
    sumLen = (mSampleCount+65535)/65536;

    //we can use the values previously calculated for the 256 summaries to shrink the loop time.
    for (int i = 0; i<sumLen; i++) {
        min = summary256[i*fields*256];
        max = summary256[i*fields*256+1];
        sumSq = summary256[i*fields*256+2];
        sumSq *= sumSq;

        for (int j = 1; j<256; j++) {
            min = std::min(min, summary256[fields*(i*256+j)]);
            max = std::max(max, summary256[fields*(i*256+j)+1]);

            float r1 = summary256[fields*(i*256+j)+2];
            sumSq += r1*r1;
        }

        float denom = i<sumLen-1 ? 256 : summaries-fractions;
        float rms = sqrt(sumSq/denom);

        summary64k[i*fields] = min;
        summary64k[i*fields+1] = max;
        summary64k[i*fields+2] = rms;
    }

    min = summary64k[0];
    max = summary64k[1];

    for (int i = 0; i< sumLen; i++) {
        min = std::min(summary64k[i*fields], min);
        max = std::max(summary64k[i*fields+1], max);
    }

    mSumMin = min;
    mSumMax = max;

    return true;
}

Sizes SqliteSampleBlock::SetSizes(size_t numSamples, SampleFormat srcFormat) {
    mSampleFormat = srcFormat;
    mSampleCount = numSamples;
    mSampleBytes = SAMPLE_SIZE(srcFormat)*mSampleCount;

    int frames64k = (mSampleCount+65535)/65536;
    int frames256 = frames64k*256;

    return {frames256*bytesPerFrame, frames64k*bytesPerFrame};
}




void SqliteSampleBlock::lock() {
    mLocked = true;
}

DBConnection *SqliteSampleBlock::Conn() {
    if (!mFactory) {
        return nullptr;
    }
    return mFactory.get()->DBConn();
}

sqlite3 *SqliteSampleBlock::DB() {
    return Conn()->DB();
}


bool SqliteSampleBlock::isSilent() {
    return mBlockID <= 0;
}

bool SqliteSampleBlock::GetSummaries(float *dest, size_t offset, size_t nFrames, DBConnection::statementID id, const char *sql) {
    bool silent = isSilent();

    //not Silent
    if (!silent) {
        auto stmt = Conn()->Prepare(id, sql);

        GetBlob(stmt, dest, floatSample, floatSample, offset*fields*SAMPLE_SIZE(floatSample), nFrames*fields*SAMPLE_SIZE(floatSample));

        return true;
    }

    memset(dest, 0, nFrames*fields*SAMPLE_SIZE(floatSample));
    return silent;
}

bool SqliteSampleBlock::GetSummary64k(float *dest, size_t offset, size_t nFrames) {
    return GetSummaries(dest, offset, nFrames, DBConnection::GetSummary64k, "SELECT summary64k FROM sampleBlocks WHERE blockID = ?1;");
}

bool SqliteSampleBlock::GetSummary256(float *dest, size_t offset, size_t nFrames) {
    return GetSummaries(dest, offset, nFrames, DBConnection::GetSummary256, "SELECT summary256 FROM sampleBlocks WHERE blockID = ?1;");
}

size_t SqliteSampleBlock::DoGetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) {
    if (isSilent()) {
        memset(dest, 0, SAMPLE_SIZE(destFormat)*nSamples);
        return nSamples;
    }

    auto* stmt = Conn()->Prepare(DBConnection::statementID::GetSamples, "SELECT samples FROM sampleBlocks WHERE blockID = ?1;");

    return GetBlob(stmt, dest, destFormat, mSampleFormat, offset, nSamples*SAMPLE_SIZE(destFormat))/SAMPLE_SIZE(destFormat);
}

MaxMinRMS SqliteSampleBlock::DoGetMaxMinRMS() {
    return {(float)mSumMax, (float)mSumMin, (float)mSumRMS};
}

MaxMinRMS SqliteSampleBlock::DoGetMaxMinRMS(size_t start, size_t len) {
    if (isSilent()) {
        return {};
    }

    float min = FLT_MAX;
    float max = - FLT_MAX;
    float sumSQ = 0.0f;

    if (!mValid) {
        load(mBlockID);
    }
    if (start < mSampleCount) {
        len = std::min(len, mSampleCount - start);

        SampleBuffer blockData(len, mSampleFormat);
        float* samples = (float*)blockData.ptr();

        size_t copied = GetSamples((samplePtr)samples, floatSample, start, len);

        for (int i = 0; i < copied; ++i, ++samples) {
            float sample = *samples;

            min = std::min(min,sample);
            max = std::max(max,sample);
            sumSQ += sample*sample;
        }
    }
    return {max, min, (float)sqrt(sumSQ/len)};

}

BlockSampleView SqliteSampleBlock::GetFloatSampleView() {

    auto cache = mCache.lock();
    if (cache) {
        return cache;
    }

    std::lock_guard<std::mutex> lock(mCacheMutex);

    const auto newCache = std::make_shared<std::vector<float>>(mSampleCount);
    const auto cachedSize = GetSamples(reinterpret_cast<samplePtr>(newCache->data()), floatSample, 0, mSampleCount);
    assert(cachedSize == mSampleCount);

    mCache = newCache;
    return newCache;
}

size_t SqliteSampleBlock::GetBlob(sqlite3_stmt *stmt, void *dest, SampleFormat destFormat, SampleFormat srcFormat, size_t srcOffset, size_t srcBytes) {
    int err;
    assert(!isSilent());

    if (sqlite3_bind_int64(stmt, 1, mBlockID)) {
        //BINDING ERROR OCCURED
        throw;
    }

    int minBytes =0;

    //Perform step
    err = sqlite3_step(stmt);

    if (err != SQLITE_ROW) {
        //something failed so rollback statment
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);

        throw;
    }

    samplePtr src = (samplePtr) sqlite3_column_blob(stmt, 0);
    size_t BlobBytes = sqlite3_column_bytes(stmt, 0);

    srcOffset = std::min(srcOffset, BlobBytes);
    minBytes = std::min(srcBytes, BlobBytes-srcOffset);

    CopySamples(src +srcOffset, srcFormat, (samplePtr) dest, destFormat, minBytes/SAMPLE_SIZE(srcFormat), none);

    dest = ((samplePtr)dest) + minBytes;
    if (srcBytes-minBytes) {
        memset(dest, 0, srcBytes-minBytes);
    }

    sqlite3_clear_bindings(stmt);

    sqlite3_reset(stmt);

    return srcBytes;
}









