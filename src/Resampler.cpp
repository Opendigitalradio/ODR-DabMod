/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Resampler.h"
#include "PcDebug.h"

#include <malloc.h>
#include <sys/types.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>

#define FFT_REAL(x) x[0]
#define FFT_IMAG(x) x[1]

template<class T>
T gcd(T a, T b)
{
    if (b == 0) {
        return a;
    }

    return gcd(b, a % b);
}


Resampler::Resampler(size_t inputRate, size_t outputRate, size_t resolution) :
    ModCodec(ModFormat(inputRate * sizeof(complexf)),
            ModFormat(outputRate * sizeof(complexf))),
    myFftPlan1(NULL),
    myFftPlan2(NULL),
    myFftIn(NULL),
    myFftOut(NULL),
    myBufferIn(NULL),
    myBufferOut(NULL),
    myFront(NULL),
    myBack(NULL),
    myWindow(NULL)
{
    PDEBUG("Resampler::Resampler(%zu, %zu) @ %p\n", inputRate, outputRate, this);

    size_t divisor = gcd(inputRate, outputRate);
    L = outputRate / divisor;
    M = inputRate / divisor;
    PDEBUG(" gcd: %zu, L: %zu, M: %zu\n", divisor, L, M);
    {
        size_t factor = resolution * 2 / M;
        if (factor & 1) {
            ++factor;
        }
        myFftSizeIn = factor * M;
        myFftSizeOut = factor * L;
    }
    PDEBUG(" FFT size in: %zu, FFT size out: %zu\n", myFftSizeIn, myFftSizeOut);

    if (myFftSizeIn > myFftSizeOut) {
        myFactor = 1.0f / myFftSizeIn;
    } else {
        myFactor = 1.0f / myFftSizeOut;
    }

    myWindow = (float*)memalign(16, myFftSizeIn * sizeof(float));
    for (size_t i = 0; i < myFftSizeIn; ++i) {
        myWindow[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (myFftSizeIn - 1)));
        PDEBUG("Window[%zu] = %f\n", i, myWindow[i]);
    }

    myFftIn = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn);
    myFront = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn);
    myFftPlan1 = fftwf_plan_dft_1d(myFftSizeIn,
            myFftIn, myFront,
            FFTW_FORWARD, FFTW_MEASURE);

    myBack = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut);
    myFftOut = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut);
    myFftPlan2 = fftwf_plan_dft_1d(myFftSizeOut,
            myBack, myFftOut,
            FFTW_BACKWARD, FFTW_MEASURE);

    myBufferIn = (complexf*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn / 2);
    myBufferOut = (complexf*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut / 2);

    memset(myBufferIn, 0, myFftSizeIn / 2 * sizeof(FFT_TYPE));
    memset(myBufferOut, 0, myFftSizeOut / 2 * sizeof(FFT_TYPE));
}


Resampler::~Resampler()
{
    PDEBUG("Resampler::~Resampler() @ %p\n", this);

    if (myFftIn != NULL) { fftwf_free(myFftIn); }
    if (myFftOut != NULL) { fftwf_free(myFftOut); }
    if (myBufferIn != NULL) { fftwf_free(myBufferIn); }
    if (myBufferOut != NULL) { fftwf_free(myBufferOut); }
    if (myFront != NULL) { fftwf_free(myFront); }
    if (myBack != NULL) { fftwf_free(myBack); }
    if (myWindow != NULL) { fftwf_free(myWindow); }
    fftwf_destroy_plan(myFftPlan1);
    fftwf_destroy_plan(myFftPlan2);
}


int Resampler::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("Resampler::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength() * L / M);

    FFT_TYPE* in = reinterpret_cast<FFT_TYPE*>(dataIn->getData());
    FFT_TYPE* out = reinterpret_cast<FFT_TYPE*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);

    for (size_t i = 0, j = 0; i < sizeIn; i += myFftSizeIn / 2, j += myFftSizeOut / 2) {
        memcpy(myFftIn, myBufferIn, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        memcpy(myFftIn + (myFftSizeIn / 2), in + i, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        memcpy(myBufferIn, in + i, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        for (size_t k = 0; k < myFftSizeIn; ++k) {
            FFT_REAL(myFftIn[k]) *= myWindow[k];
            FFT_IMAG(myFftIn[k]) *= myWindow[k];
        }

        fftwf_execute(myFftPlan1);

        if (myFftSizeOut > myFftSizeIn) {
            memset(myBack, 0, myFftSizeOut * sizeof(FFT_TYPE));
            memcpy(myBack, myFront, myFftSizeIn / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut - (myFftSizeIn / 2)],
                    &myFront[myFftSizeIn / 2],
                    myFftSizeIn / 2 * sizeof(FFT_TYPE));
            // Copy input Fs
            FFT_REAL(myBack[myFftSizeIn / 2]) =
                FFT_REAL(myFront[myFftSizeIn / 2]);
            FFT_IMAG(myBack[myFftSizeIn / 2]) =
                FFT_IMAG(myFront[myFftSizeIn / 2]);
        } else {
            memcpy(myBack, myFront, myFftSizeOut / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut / 2],
                    &myFront[myFftSizeIn - (myFftSizeOut / 2)],
                    myFftSizeOut / 2 * sizeof(FFT_TYPE));
            // Average output Fs from input
            FFT_REAL(myBack[myFftSizeOut / 2]) +=
                FFT_REAL(myFront[myFftSizeOut / 2]);
            FFT_IMAG(myBack[myFftSizeOut / 2]) +=
                FFT_IMAG(myFront[myFftSizeOut / 2]);
            FFT_REAL(myBack[myFftSizeOut / 2]) *= 0.5f;
            FFT_IMAG(myBack[myFftSizeOut / 2]) *= 0.5f;
        }
        for (size_t k = 0; k < myFftSizeOut; ++k) {
            FFT_REAL(myBack[k]) *= myFactor;
            FFT_IMAG(myBack[k]) *= myFactor;
        }

        fftwf_execute(myFftPlan2);

        for (size_t k = 0; k < myFftSizeOut / 2; ++k) {
            FFT_REAL(out[j + k]) = myBufferOut[k].real() + FFT_REAL(myFftOut[k]);
            FFT_IMAG(out[j + k]) = myBufferOut[k].imag() + FFT_IMAG(myFftOut[k]);
        }
        memcpy(myBufferOut, myFftOut + (myFftSizeOut / 2), (myFftSizeOut / 2) * sizeof(FFT_TYPE));
    }

    return 1;
}

