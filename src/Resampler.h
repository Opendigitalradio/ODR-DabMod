/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef RESAMPLER_H
#define RESAMPLER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "porting.h"
#include "ModCodec.h"
#include "kiss_fftsimd.h"


#include <sys/types.h>
#include <kiss_fft.h>
#include <tools/kiss_fftr.h>
#include <complex>
typedef std::complex<float> complexf;


class Resampler : public ModCodec
{
public:
    Resampler(size_t inputRate, size_t outputRate, size_t resolution = 512);
    virtual ~Resampler();
    Resampler(const Resampler&);
    Resampler& operator=(const Resampler&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "Resampler"; }

protected:
    FFT_PLAN myFftPlan1;
    FFT_PLAN myFftPlan2;
    size_t L;
    size_t M;
    size_t K;
    size_t myFftSizeIn;
    size_t myFftSizeOut;
    FFT_TYPE* myFftIn;
    FFT_TYPE* myFftOut;
    complexf* myBufferIn;
    complexf* myBufferOut;
    FFT_TYPE* myFront;
    FFT_TYPE* myBack;
    float *myWindow;
    float myFactor;
};


#endif // RESAMPLER_H
