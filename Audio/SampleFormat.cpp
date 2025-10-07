/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "SampleFormat.h"

#include <string.h>
#include <wx/debug.h>

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
    gDitherAlgorithm.Apply(dither, src, srcFormat, dst, dstFormat, len, srcStride, dstStride);
}

void SamplesToFloat(constSamplePtr src, SampleFormat srcFormat, float* dst, size_t len, size_t srcStride /* =1 */, size_t dstStride /* =1 */) {
    CopySamples(src, srcFormat, reinterpret_cast<samplePtr>(dst), floatSample, len, none, srcStride, dstStride);
}
void ReverseSamples(samplePtr dst, SampleFormat format, size_t start, size_t len) {
    auto size = SAMPLE_SIZE(format);
    samplePtr first = dst + start * size;
    samplePtr last = dst + (start + len - 1) * size;
    enum : size_t { fixedSize = SAMPLE_SIZE(floatSample) };
    wxASSERT(static_cast<size_t>(size) <= fixedSize);
    char temp[fixedSize];
    while (first < last) {
        memcpy(temp, first, size);
        memcpy(first, last, size);
        memcpy(last, temp, size);
        first += size;
        last -= size;
    }
}



