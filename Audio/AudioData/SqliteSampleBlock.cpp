//
// Created by cline on 2025-03-09.
//

#include "SqliteSampleBlock.h"

#include <cmath>
#include <cfloat>
#include <iostream>

#include "../Dither.h"
#include "../IO/AudioIO.h"


std::map<SampleBlockID, std::shared_ptr<SqliteSampleBlock>> SqliteSampleBlockFactory::sSilentBlocks;

//FACTORY FUNCTIONS
SqliteSampleBlockFactory::SqliteSampleBlockFactory() {
    mDB = AudioIOBase::sAudioDB;
}
SqliteSampleBlockFactory::~SqliteSampleBlockFactory() {
    std::cout<<"TEST2"<<std::endl;
}

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
    std::cout<<"Test"<<std::endl;

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

void SqliteSampleBlock::Commit(Sizes sizes) {

}

void SqliteSampleBlock::load(SampleBlockID id) {

}

void SqliteSampleBlock::Delete() {

}

size_t SqliteSampleBlock::DoGetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) {
    if (!isSilent()) {
        memset(dest, 0, SAMPLE_SIZE(destFormat)*nSamples);
        return nSamples;
    }

    auto* stmt = Conn()->Prepare(DBConnection::statementID::GetSamples, "SELECT samples FROM sampleBlocks WHERE blockID = ?1");

    return GetBlob(stmt, dest, destFormat, mSampleFormat, offset, nSamples*SAMPLE_SIZE(destFormat))/SAMPLE_SIZE(destFormat);
}

MaxMinRMS SqliteSampleBlock::DoGetMaxMinRMS() {
    return {(float)mSumMax, (float)mSumMin, (float)mSumRMS};
}

MaxMinRMS SqliteSampleBlock::DoGetMaxMinRMS(size_t start, size_t len) {
    return {};
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
    minBytes = std::min(srcOffset, srcBytes-BlobBytes);

    CopySamples(src +srcOffset, srcFormat, (samplePtr) dest, destFormat, minBytes/SAMPLE_SIZE(srcFormat), none);

    dest = ((samplePtr)dest) + minBytes;
    if (srcBytes-minBytes) {
        memset(dest, 0, srcBytes-minBytes);
    }

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    return srcBytes;
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

bool SqliteSampleBlock::GetSummary64k(float *dest, size_t offset, size_t nFrames) {
    return false;
}

bool SqliteSampleBlock::GetSummary256(float *dest, size_t offset, size_t nFrames) {
    return false;
}
BlockSampleView SqliteSampleBlock::GetFloatSampleView() {
    return{};
}
bool SqliteSampleBlock::GetSummaries(float *dest, size_t offset, size_t nFrames, DBConnection::statementID id, const char *sql) {
    return false;
}











