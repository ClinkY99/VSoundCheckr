//
// Created by cline on 2025-03-09.
//

#ifndef SAMPLEBLOCK_H
#define SAMPLEBLOCK_H
#include <memory>
#include <unordered_set>

#include "../SampleFormat.h"

using SampleBlockID = long long;
using SampleBlockIDs = std::unordered_set<SampleBlockID>;

struct MaxMinRMS {
     float max = 0;
     float min = 0;
     float RMS = 0;
};

class SampleBlock {
public:
     virtual ~SampleBlock() = default;

     virtual void lock() = 0;
     virtual bool isSilent() =0;
     virtual SampleBlockID getBlockID() = 0;
     bool GetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples);

     virtual bool GetSummary256(float* dest, size_t offset, size_t nFrames);
     virtual bool GetSummary64k(float* dest, size_t offset, size_t nFrames);

     //for section of block
     MaxMinRMS GetMaxMinRMS(size_t start, size_t len);
     //for entire block
     MaxMinRMS GetMaxMinRMS();

protected:
     virtual bool DoGetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) = 0;
     virtual MaxMinRMS DoGetMaxMinRMS(size_t start, size_t len) = 0;
     virtual MaxMinRMS DoGetMaxMinRMS();


};

using SampleBlockPtr = std::shared_ptr<SampleBlock>;

class SampleBlockFactory {
public:
     virtual ~SampleBlockFactory() = default;

     SampleBlockPtr Create(constSamplePtr src, SampleFormat srcFormat, size_t numSamples);
     SampleBlockPtr CreateFromID(SampleFormat srcFormat, SampleBlockID srcBlockID);
     SampleBlockPtr CreateSilent(size_t nSamples, SampleFormat srcFormat);
protected:
     virtual SampleBlockPtr DoCreate(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) = 0;
     virtual SampleBlockPtr DoCreateID(SampleFormat srcFormat, SampleBlockID srcBlockID) = 0;
     virtual SampleBlockPtr DoCreateSilent(size_t nSamples, SampleFormat srcFormat) = 0;
};




#endif //SAMPLEBLOCK_H
