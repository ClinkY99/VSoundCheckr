//
// Created by cline on 2025-03-09.
//

#include "SampleBlock.h"

size_t SampleBlock::GetSamples(samplePtr dest, SampleFormat destFormat, size_t offset, size_t nSamples) {
    try{return DoGetSamples(dest, destFormat, offset, nSamples);}
    catch (...) {
        if (true) {
            throw;
        }
        ClearSamples(dest,destFormat,0,nSamples);
    }
}

MaxMinRMS SampleBlock::GetMaxMinRMS(size_t start, size_t len) {
    try{return DoGetMaxMinRMS(start, len);}
    catch (...) {
        if (true) {
            throw;
        }
        return {};
    }
}
MaxMinRMS SampleBlock::GetMaxMinRMS() {
    try{return DoGetMaxMinRMS();}
    catch (...) {
        if (true) {
            throw;
        }
        return {};
    }


}

SampleBlockPtr SampleBlockFactory::Create(constSamplePtr src, SampleFormat srcFormat, size_t numSamples) {
    auto result = DoCreate(src, srcFormat, numSamples);
    if (!result);
        //Throw Error Here
    return result;
}

SampleBlockPtr SampleBlockFactory::CreateFromID(SampleFormat srcFormat, SampleBlockID srcBlockID) {
    auto result = DoCreateID(srcFormat, srcBlockID);
    if (!result);
        //Throw Error Here
    return result;
}

SampleBlockPtr SampleBlockFactory::CreateSilent(size_t nSamples, SampleFormat srcFormat) {
    auto result = DoCreateSilent(nSamples, srcFormat);
    if (!result)
        throw;
    return result;
}

