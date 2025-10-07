/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "Resample.h"

#include <soxr.h>
#include <wx/chartype.h>

Resample::Resample(const bool useBestMethod, const double dMinFactor, const double dMaxFactor)
{
   this->SetMethod(useBestMethod);
   soxr_quality_spec_t q_spec;
   if (dMinFactor == dMaxFactor)
   {
      mbWantConstRateResampling = true; // constant rate resampling
      q_spec = soxr_quality_spec("\0\1\4\6"[mMethod], 0);
   }
   else
   {
      mbWantConstRateResampling = false; // variable rate resampling
      q_spec = soxr_quality_spec(SOXR_HQ, SOXR_VR);
   }
   mHandle.reset(soxr_create(1, dMinFactor, 1, 0, 0, &q_spec, 0));
}

Resample::~Resample()
{
}

std::pair<size_t, size_t>
      Resample::Process(double       factor,
                        const float *inBuffer,
                        size_t       inBufferLen,
                        bool         lastFlag,
                        float       *outBuffer,
                        size_t       outBufferLen)
{
   size_t idone, odone;
   if (mbWantConstRateResampling)
   {
      soxr_process(mHandle.get(),
            inBuffer , (lastFlag? ~inBufferLen : inBufferLen), &idone,
            outBuffer,                           outBufferLen, &odone);
   }
   else
   {
      soxr_set_io_ratio(mHandle.get(), 1/factor, 0);

      inBufferLen = lastFlag? ~inBufferLen : inBufferLen;
      soxr_process(mHandle.get(),
            inBuffer , inBufferLen , &idone,
            outBuffer, outBufferLen, &odone);
   }
   return { idone, odone };
}

//temporary until settings
void Resample::SetMethod(const bool useBestMethod)
{
   if (useBestMethod)
      mMethod = 3;
   else
      mMethod = 1;
}