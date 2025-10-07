/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef RESAMPLE_H
#define RESAMPLE_H
#include <memory>

template< typename Enum > class EnumSetting;

struct soxr;
extern "C" void soxr_delete(soxr*);
struct soxr_deleter {
    void operator () (soxr *p) const { if (p) soxr_delete(p); }
};
using soxrHandle = std::unique_ptr<soxr, soxr_deleter>;

class Resample final
{
public:
    //
    // dMinFactor and dMaxFactor specify the range of factors for variable-rate resampling.
    // For constant-rate, pass the same value for both.
    Resample(const bool useBestMethod, const double dMinFactor, const double dMaxFactor);
    ~Resample();

    Resample( Resample&&) noexcept = default;
    Resample& operator=(Resample&&) noexcept = default;

    Resample(const Resample&) = delete;
    Resample& operator=(const Resample&) = delete;

    std::pair<size_t, size_t>
                 Process(double       factor,
                         const float *inBuffer,
                         size_t       inBufferLen,
                         bool         lastFlag,
                         float       *outBuffer,
                         size_t       outBufferLen);

protected:
    void SetMethod(const bool useBestMethod);

protected:
    int   mMethod; // resampler-specific enum for resampling method
    soxrHandle mHandle; // constant-rate or variable-rate resampler (XOR per instance)
    bool mbWantConstRateResampling;
};



#endif //RESAMPLE_H
