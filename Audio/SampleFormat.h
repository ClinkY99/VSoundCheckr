/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef SAMPLEFORMAT_H
#define SAMPLEFORMAT_H

#pragma once
#include <algorithm>

enum DitherType : unsigned;
extern DitherType gLowQualDither,gHighQualDither;

enum class SampleFormat : unsigned {
    undefinedSample = 0,
    int16Sample = 0x00020001,
    int24Sample = 0x00040001,
    floatSample = 0x0004000F,

    narrowestSampleFormat = int16Sample,
    widestSampleFormat = floatSample,
};

constexpr SampleFormat undefinedSample = SampleFormat::undefinedSample;
constexpr SampleFormat int16Sample = SampleFormat::int16Sample;
constexpr SampleFormat int24Sample = SampleFormat::int24Sample;
constexpr SampleFormat floatSample = SampleFormat::floatSample;
constexpr SampleFormat narrowestSampleFormat = SampleFormat::narrowestSampleFormat;
constexpr SampleFormat widestSampleFormat = SampleFormat::widestSampleFormat;

// ----------------------------------------------------------------------------
// Provide the number of bytes a specific sample will take
// ----------------------------------------------------------------------------
#define SAMPLE_SIZE(SampleFormat) (static_cast<unsigned>(SampleFormat) >> 16)

//Pointers to Sample data
using samplePtr = char *;
using constSamplePtr = const char *;


class SampleFormats final{

    //class vars
    SampleFormat mEffective;
    SampleFormat mStored;

public:
    SampleFormats(SampleFormat effective, SampleFormat stored)
        : mEffective{effective}, mStored{stored} {}

    SampleFormat getEffective() const {return mEffective;}
    SampleFormat getStored() const {return mStored;}

    void updateEffective(SampleFormat effective) {
        if (effective > mEffective) {
            mEffective = std::min(effective, mStored);
        }
    }
};

// Operators for sample formats

inline bool operator == (SampleFormats a, SampleFormats b)
{
    return a.getEffective() == b.getEffective() &&
       a.getStored() == b.getStored();
}

inline bool operator != (SampleFormats a, SampleFormats b)
{
    return !(a == b);
}


//Sample Buffer things now
class SampleBuffer {

    samplePtr mPtr;

public:
    SampleBuffer()
        : mPtr(0) {}
    SampleBuffer(size_t count, SampleFormat format)
        : mPtr((samplePtr)malloc(count * SAMPLE_SIZE(format))) {}

    ~SampleBuffer() {
        Free();
    }

    SampleBuffer(SampleBuffer &&other) {
        mPtr = other.mPtr;
        other.mPtr = nullptr;
    }

    SampleBuffer &operator=(SampleBuffer &&other) {
        mPtr = other.mPtr;
        other.mPtr = nullptr;
        return *this;
    }

    //WARNING!!! Might not preserve previous contents

    SampleBuffer &Allocate(size_t count, SampleFormat format) {
        Free();
        mPtr = (samplePtr)malloc(count * SAMPLE_SIZE(format));
        return *this;
    }

    void Free() {
        free(mPtr);
        mPtr = 0;
    }

    samplePtr ptr() const {return mPtr;}
};

class GrowableSampleBuffer : SampleBuffer {
    size_t mCount;

public:
    GrowableSampleBuffer()
        : mCount(0) {}
    GrowableSampleBuffer(size_t count, SampleFormat format)
        : SampleBuffer(count, format), mCount(count) {}

    SampleBuffer &ReSize(size_t count, SampleFormat format) {
        if (!ptr() || mCount <count) {
            Allocate(count, format);
            mCount = count;
        }

        return *this;
    }

    void Free() {
        SampleBuffer::Free();
        mCount = 0;
    }

    using SampleBuffer::ptr;
};

void InitDithers();
void ClearSamples(samplePtr dst, SampleFormat format, size_t start, size_t len);
void ReverseSamples(samplePtr dst, SampleFormat format, size_t start, size_t len);
void CopySamples(constSamplePtr src, SampleFormat srcFormat, samplePtr dst, SampleFormat dstFormat, size_t len,DitherType dither, size_t srcStride = 1, size_t dstStride = 1);
void SamplesToFloat(constSamplePtr src, SampleFormat srcFormat, float* dst, size_t len, size_t srcStride = 1, size_t dstStride = 1);



#endif //SAMPLEFORMAT_H
