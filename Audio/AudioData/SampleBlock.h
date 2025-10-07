/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef SAMPLEBLOCK_H
#define SAMPLEBLOCK_H
#include <memory>
#include <unordered_set>
#include <vector>

#include "../SampleFormat.h"

using SampleBlockID = long long;
using SampleBlockIDs = std::unordered_set<SampleBlockID>;
using BlockSampleView = std::shared_ptr<std::vector<float>>;



struct MaxMinRMS {
     float max = 0;
     float min = 0;
     float RMS = 0;

};

class SampleBlock {
public:
     virtual void lock() = 0;
     virtual bool isSilent() =0;
     virtual SampleBlockID getBlockID() = 0;
     virtual SampleFormat getSampleFormat() = 0;
     virtual size_t getSampleCount() = 0;
     size_t GetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples);

     virtual bool GetSummary256(float* dest, size_t offset, size_t nFrames)= 0;
     virtual bool GetSummary64k(float* dest, size_t offset, size_t nFrames) = 0;



     virtual BlockSampleView GetFloatSampleView() = 0;
     //for section of block
     MaxMinRMS GetMaxMinRMS(size_t start, size_t len);
     //for entire block
     MaxMinRMS GetMaxMinRMS();

protected:
     virtual size_t DoGetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) = 0;
     virtual MaxMinRMS DoGetMaxMinRMS(size_t start, size_t len) = 0;
     virtual MaxMinRMS DoGetMaxMinRMS() = 0;


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
