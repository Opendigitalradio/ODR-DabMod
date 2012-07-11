/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GAIN_CONTROL_H
#define GAIN_CONTROL_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"


#include <sys/types.h>
#include <complex>
#ifdef __SSE__
#   include <xmmintrin.h>
#endif


typedef std::complex<float> complexf;

enum GainMode { GAIN_FIX, GAIN_MAX, GAIN_VAR };

class GainControl : public ModCodec
{
public:
    GainControl(size_t frameSize, GainMode mode = GAIN_VAR, float factor = 1.0f);
    virtual ~GainControl();
    GainControl(const GainControl&);
    GainControl& operator=(const GainControl&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "GainControl"; }

protected:
    size_t d_frameSize;
    float d_factor;
#ifdef __SSE__
    __m128 (*computeGain)(const __m128* in, size_t sizeIn);
    __m128 static computeGainFix(const __m128* in, size_t sizeIn);
    __m128 static computeGainMax(const __m128* in, size_t sizeIn);
    __m128 static computeGainVar(const __m128* in, size_t sizeIn);
#else // !__SSE__
    float (*computeGain)(const complexf* in, size_t sizeIn);
    float static computeGainFix(const complexf* in, size_t sizeIn);
    float static computeGainMax(const complexf* in, size_t sizeIn);
    float static computeGainVar(const complexf* in, size_t sizeIn);
#endif // __SSE__
};


#endif // GAIN_CONTROL_H
