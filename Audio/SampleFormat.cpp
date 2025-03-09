//
// Created by cline on 2025-02-24.
//

#include "SampleFormat.h"

#include <string.h>

#include "Dither.h"

DitherType gLowQualDither = DitherType::none;
DitherType gHighQualDither = DitherType::shaped;
static Dither gDitherAlgorithm;

void InitDithers() {
    gLowQualDither = Dither::FastDitherType();
    gHighQualDither = Dither::BestDitherType();
}

void ClearSamples(samplePtr dst, SampleFormat format, size_t start, size_t len) {
    auto size = SAMPLE_SIZE(format);
    memset(dst+(start*size), 0, len*size);
}

void CopySamples(constSamplePtr src, SampleFormat srcFormat, samplePtr dst, SampleFormat dstFormat, size_t len,DitherType dither, size_t srcStride /* =1 */, size_t dstStride /* =1 */) {
    gDitherAlgorithm.Apply(dither, src, srcFormat, dst, dstFormat,len, srcStride, dstStride);
}


