/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef DITHER_H
#define DITHER_H

#include "SampleFormat.h"

enum DitherType : unsigned {
    none = 0,
    rectangle = 1,
    triangle = 2,
    shaped = 3
};

class Dither {
public:
    Dither();

    void Reset();
    void Apply(DitherType type, constSamplePtr src, SampleFormat srcFormat, samplePtr dst, SampleFormat dstFormat, unsigned int len, unsigned int srcStride = 1, unsigned int dstStride = 1);

    static DitherType FastDitherType();
    static DitherType BestDitherType();

    // static EnumSettings<DitherType> FastSetting;
    // static EnumSettings<DitherType> BestSetting;
};





#endif //DITHER_H
