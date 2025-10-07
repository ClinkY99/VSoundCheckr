/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "Dither.h"
#include <wx/defs.h>

#include "../MemoryManagement/Math/float_cast.h"

// Constants for the noise shaping buffer
constexpr int BUF_SIZE = 8;
constexpr int BUF_MASK = 7;

// Lipshitz's minimally audible FIR
const float SHAPED_BS[] = { 2.033f, -2.165f, 1.959f, -1.590f, 0.6149f };

// Dither state
struct State {
    int mPhase;
    float mTriangleState;
    float mBuffer[8 /* = BUF_SIZE */];
} mState;

using Ditherer = float (*)(State&, float);

// Definitions for sample conversions
constexpr auto CONVERT_DIV16 = float(1<<15);
constexpr auto CONVERT_DIV24 = float(1<<23);

static inline float DITHER_NOISE() {
    return rand() / (float) RAND_MAX - 0.5;
}

static inline float FROM_INT16(const short *ptr) {
    return *ptr / CONVERT_DIV16;
}

static inline float FROM_INT24(const int *ptr) {
    return *ptr / CONVERT_DIV24;
}

static inline float FROM_FLOAT(const float *ptr) {
    return std::clamp(*ptr, -1.0f, 1.0f);
}

template <typename dst_type>
static inline void IMPLEMENT_STORE(dst_type *ptr, float sample, dst_type min_bound, dst_type max_bound) {
    int x = lrintf(sample);
    if (x>max_bound) x = max_bound;
    else if (x<min_bound) x = min_bound;
    *ptr = static_cast<dst_type>(x);
}

// Dither single float 'sample' and store it in pointer 'dst', using 'dither' as algorithm
static inline void DITHER_TO_INT16(Ditherer dither, State& state, short *dst, float sample) {
    IMPLEMENT_STORE<short>(dst, dither(state, sample * CONVERT_DIV16),short(-32768), short(32767));
}

static inline void DITHER_TO_INT24(Ditherer dither, State& state, int *dst, float sample) {
    IMPLEMENT_STORE<int>(dst, dither(state, sample*CONVERT_DIV24), -8388608, 8388607);
}

//Implement Dither loop
template<typename srcType, typename dstType>
static inline void DITHER_LOOP(Ditherer dither, State& state,
    void (*store)(Ditherer, State&, dstType*, float),
    float (*load)(const srcType*),
    samplePtr dst, SampleFormat dstFormat, size_t dstStride,
    constSamplePtr src, SampleFormat srcFormat, size_t srcStride, size_t len){
    char *d;
    const char *s;
    unsigned int i;
    for (i = 0, d = (char*)dst, s = (char*)src; i < len; i++, d+=SAMPLE_SIZE(dstFormat)*dstStride, s+=SAMPLE_SIZE(srcFormat)*srcStride) {
        store(dither, state, reinterpret_cast<dstType *> (d), load((srcType*)s));
    }
}

// Implement a dither. There are only 3 cases where we must dither,
// in all other cases, no dithering is necessary.
static inline void DITHER(Ditherer dither, State& state,
    samplePtr dst, SampleFormat dstFormat, size_t dstStride,
    constSamplePtr src, SampleFormat srcFormat, size_t srcStride, size_t len) {
    if (srcFormat == int24Sample && dstFormat == int16Sample) {
        DITHER_LOOP<int, short>(dither, state, DITHER_TO_INT16, FROM_INT24, dst, int16Sample,dstStride,src,int24Sample,srcStride,len);
    } else if (srcFormat == floatSample && dstFormat == int16Sample) {
        DITHER_LOOP<float, short>(dither, state, DITHER_TO_INT16, FROM_FLOAT, dst, int16Sample,dstStride,src,floatSample,srcStride,len);
    } else if (srcFormat == floatSample && dstFormat == int24Sample) {
        DITHER_LOOP<float,int>(dither,state, DITHER_TO_INT24, FROM_FLOAT, dst, int24Sample,dstStride,src,floatSample,srcStride,len);
    } else {
        wxASSERT(false);
    }
}

static inline float NoDither(State &, float sample);
static inline float RectangleDither(State &, float sample);
static inline float TriangleDither(State &state, float sample);
static inline float ShapedDither(State &state, float sample);

Dither::Dither() {
    //on setup reset values
    Reset();
}

void Dither::Reset() {
    mState.mPhase = 0;
    mState.mTriangleState = 0;
    memset(mState.mBuffer, 0, sizeof(float)*BUF_SIZE);
}

void Dither::Apply(DitherType type,
    constSamplePtr src, SampleFormat srcFormat,
    samplePtr dst, SampleFormat dstFormat,
    unsigned int len,
    unsigned int srcStride /** = 1**/,
    unsigned int dstStride /** = 1 **/) {

    unsigned int i;

    //ensure running on the correct base op system
    wxASSERT(sizeof(int) == 4);
    wxASSERT(sizeof(short) == 2);

    //check params (testing)
    wxASSERT(src);
    wxASSERT(dst);
    wxASSERT(srcStride > 0);
    wxASSERT(dstStride > 0);

    //nothing to do
    if (len == 0) {
        return;
    }
    if (srcFormat==dstFormat) {
        if (srcStride == 1 && dstStride == 1) {
            memcpy(dst, src, len*SAMPLE_SIZE(dstFormat));
        } else {
            if (dstFormat == floatSample) {
                auto d = (float*) dst;
                auto s = (const float*) src;

                for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
                    *d=*s;
                }
            } else if (dstFormat == int24Sample) {
                auto d = (int*) dst;
                auto s = (const int*) src;
                for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
                    *d=*s;
                }
            } else if (dstFormat == int16Sample) {
                auto d = (short*) dst;
                auto s = (const short*) src;
                for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
                    *d=*s;
                }
            } else {
                //unknown sample format
                wxASSERT(false);
            }
        }
    } else if (dstFormat==floatSample) {
        if (srcFormat == int16Sample) {
            auto d = (float*) dst;
            auto s = (const short*) src;

            for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
                *d = FROM_INT16(s);
            }
        } else if (srcFormat == int24Sample) {
            auto d = (float*) dst;
            auto s = (const int*) src;

            for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
                *d = FROM_INT24(s);
            }
        } else {
            //unknown sample format
            wxASSERT(false);
        }
    } else if (dstFormat==int24Sample, srcFormat==int16Sample) {
        auto d = (float*) dst;
        auto s = (const short*) src;

        for (i = 0; i < len; i++, d+=dstStride, s+=srcStride) {
            *d = ((int)*s)<<8;
        }
    } else {
        //damn we have to dither :(
        switch (type) {
            case DitherType::none:
                DITHER(NoDither, mState, dst, dstFormat, dstStride, src, srcFormat, srcStride, len);
                break;
            case DitherType::rectangle:
                DITHER(RectangleDither, mState, dst, dstFormat, dstStride, src, srcFormat, srcStride, len);
                break;
            case DitherType::triangle:
                //needs to reset for this
                Reset();
                DITHER(TriangleDither, mState, dst, dstFormat, dstStride, src, srcFormat, srcStride, len);
                break;
            case DitherType::shaped:
                //needs to reset for this
                Reset();
                DITHER(ShapedDither,mState, dst, dstFormat, dstStride, src, srcFormat, srcStride, len);
                break;
            default:
                wxASSERT(false); //Unknown Dither algorithm
        }
    }
}

inline float NoDither(State &state, float sample) {
    return sample;
}

inline float RectangleDither(State &state, float sample) {
    return sample - DITHER_NOISE();
}

// Triangle dither - high pass filtered
inline float TriangleDither(State &state, float sample) {
    float r = DITHER_NOISE();
    float result = sample + r - state.mTriangleState;
    state.mTriangleState = r;

    return result;
}

inline float ShapedDither(State &state, float sample) {
    float r = DITHER_NOISE() + DITHER_NOISE();

    // Run FIR
    float xe = sample + state.mBuffer[state.mPhase] * SHAPED_BS[0]
        + state.mBuffer[(state.mPhase - 1) & BUF_MASK] * SHAPED_BS[1]
        + state.mBuffer[(state.mPhase - 2) & BUF_MASK] * SHAPED_BS[2]
        + state.mBuffer[(state.mPhase - 3) & BUF_MASK] * SHAPED_BS[3]
        + state.mBuffer[(state.mPhase - 4) & BUF_MASK] * SHAPED_BS[4];

    float result = r+xe;

    state.mPhase = (state.mPhase + 1) & BUF_MASK;
    state.mBuffer[state.mPhase] = xe - lrintf(result);

    return result;
}

DitherType Dither::FastDitherType() {
    //Get dithering algorithms from settings
    //for now just returns none
    return DitherType::none;
}

DitherType Dither::BestDitherType() {
    //Get dithering algorithms from settings
    //for now just returns shaped
    return DitherType::shaped;
}




