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

#include "OfdmGenerator.h"
#include "PcDebug.h"
#if USE_FFTW
#  include "fftw3.h"
#  define FFT_TYPE fftwf_complex
#else
#  include "kiss_fftsimd.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>
#include <complex>
typedef std::complex<float> complexf;


OfdmGenerator::OfdmGenerator(size_t nbSymbols,
        size_t nbCarriers,
        size_t spacing,
        bool inverse) :
    ModCodec(ModFormat(nbSymbols * nbCarriers * sizeof(FFT_TYPE)),
            ModFormat(nbSymbols * spacing * sizeof(FFT_TYPE))),
    myFftPlan(NULL),
#if USE_FFTW
    myFftIn(NULL), myFftOut(NULL),
#else
    myFftBuffer(NULL),
#endif
    myNbSymbols(nbSymbols),
    myNbCarriers(nbCarriers),
    mySpacing(spacing)
{
    PDEBUG("OfdmGenerator::OfdmGenerator(%zu, %zu, %zu, %s) @ %p\n",
            nbSymbols, nbCarriers, spacing, inverse ? "true" : "false", this);

    if (nbCarriers > spacing) {
        throw std::runtime_error(
                "OfdmGenerator::OfdmGenerator nbCarriers > spacing!");
    }

    if (inverse) {
        myPosDst = (nbCarriers & 1 ? 0 : 1);
        myPosSrc = 0;
        myPosSize = (nbCarriers + 1) / 2;
        myNegDst = spacing - (nbCarriers / 2);
        myNegSrc = (nbCarriers + 1) / 2;
        myNegSize = nbCarriers / 2;
    }
    else {
        myPosDst = (nbCarriers & 1 ? 0 : 1);
        myPosSrc = nbCarriers / 2;
        myPosSize = (nbCarriers + 1) / 2;
        myNegDst = spacing - (nbCarriers / 2);
        myNegSrc = 0;
        myNegSize = nbCarriers / 2;
    }
    myZeroDst = myPosDst + myPosSize;
    myZeroSize = myNegDst - myZeroDst;

    PDEBUG("  myPosDst: %u\n", myPosDst);
    PDEBUG("  myPosSrc: %u\n", myPosSrc);
    PDEBUG("  myPosSize: %u\n", myPosSize);
    PDEBUG("  myNegDst: %u\n", myNegDst);
    PDEBUG("  myNegSrc: %u\n", myNegSrc);
    PDEBUG("  myNegSize: %u\n", myNegSize);
    PDEBUG("  myZeroDst: %u\n", myZeroDst);
    PDEBUG("  myZeroSize: %u\n", myZeroSize);

#if USE_FFTW
    const int N = mySpacing; // The size of the FFT
    myFftIn = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * N);
    myFftOut = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * N);
    myFftPlan = fftwf_plan_dft_1d(N,
            myFftIn, myFftOut,
            FFTW_BACKWARD, FFTW_MEASURE);

    if (sizeof(complexf) != sizeof(FFT_TYPE)) {
        printf("sizeof(complexf) %zu\n", sizeof(complexf));
        printf("sizeof(FFT_TYPE) %zu\n", sizeof(FFT_TYPE));
        throw std::runtime_error(
                "OfdmGenerator::process complexf size is not FFT_TYPE size!");
    }
#else
    myFftPlan = kiss_fft_alloc(mySpacing, 1, NULL, NULL);
    myFftBuffer = (FFT_TYPE*)memalign(16, mySpacing * sizeof(FFT_TYPE));
#endif

}


OfdmGenerator::~OfdmGenerator()
{
    PDEBUG("OfdmGenerator::~OfdmGenerator() @ %p\n", this);

#if USE_FFTW
    if (myFftIn) {
         fftwf_free(myFftIn);
    }

    if (myFftOut) {
         fftwf_free(myFftOut);
    }

    if (myFftPlan) {
        fftwf_destroy_plan(myFftPlan);
    }

#else
    if (myFftPlan != NULL) {
        kiss_fft_free(myFftPlan);
    }

    if (myFftBuffer != NULL) {
        free(myFftBuffer);
    }

    kiss_fft_cleanup();
#endif
}

int OfdmGenerator::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("OfdmGenerator::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(myNbSymbols * mySpacing * sizeof(complexf));

    FFT_TYPE* in = reinterpret_cast<FFT_TYPE*>(dataIn->getData());
    FFT_TYPE* out = reinterpret_cast<FFT_TYPE*>(dataOut->getData());

    size_t sizeIn = dataIn->getLength() / sizeof(complexf);
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    if (sizeIn != myNbSymbols * myNbCarriers) {
        PDEBUG("Nb symbols: %zu\n", myNbSymbols);
        PDEBUG("Nb carriers: %zu\n", myNbCarriers);
        PDEBUG("Spacing: %zu\n", mySpacing);
        PDEBUG("\n%zu != %zu\n", sizeIn, myNbSymbols * myNbCarriers);
        throw std::runtime_error(
                "OfdmGenerator::process input size not valid!");
    }
    if (sizeOut != myNbSymbols * mySpacing) {
        PDEBUG("Nb symbols: %zu\n", myNbSymbols);
        PDEBUG("Nb carriers: %zu\n", myNbCarriers);
        PDEBUG("Spacing: %zu\n", mySpacing);
        PDEBUG("\n%zu != %zu\n", sizeIn, myNbSymbols * mySpacing);
        throw std::runtime_error(
                "OfdmGenerator::process output size not valid!");
    }

#if USE_FFTW
    // No SIMD/no-SIMD distinction, it's too early to optimize anything
    for (size_t i = 0; i < myNbSymbols; ++i) {
        myFftIn[0][0] = 0;
        myFftIn[0][1] = 0;

        bzero(&myFftIn[myZeroDst], myZeroSize * sizeof(FFT_TYPE));
        memcpy(&myFftIn[myPosDst], &in[myPosSrc],
                myPosSize * sizeof(FFT_TYPE));
        memcpy(&myFftIn[myNegDst], &in[myNegSrc],
                myNegSize * sizeof(FFT_TYPE));

        fftwf_execute(myFftPlan);

        memcpy(out, myFftOut, mySpacing * sizeof(FFT_TYPE));

        in += myNbCarriers;
        out += mySpacing;
    }
#else
#  ifdef USE_SIMD
    for (size_t i = 0, j = 0; i < sizeIn; ) {
        // Pack 4 fft operations
        typedef struct {
            float r[4];
            float i[4];
        } fft_data;
        assert(sizeof(FFT_TYPE) == sizeof(fft_data));
        complexf *cplxIn = (complexf*)in;
        complexf *cplxOut = (complexf*)out;
        fft_data *dataBuffer = (fft_data*)myFftBuffer;

        FFT_REAL(myFftBuffer[0]) = _mm_setzero_ps();
        FFT_IMAG(myFftBuffer[0]) = _mm_setzero_ps();
        for (size_t k = 0; k < myZeroSize; ++k) {
            FFT_REAL(myFftBuffer[myZeroDst + k]) = _mm_setzero_ps();
            FFT_IMAG(myFftBuffer[myZeroDst + k]) = _mm_setzero_ps();
        }
        for (int k = 0; k < 4; ++k) {
            if (i < sizeIn) {
                for (size_t l = 0; l < myPosSize; ++l) {
                    dataBuffer[myPosDst + l].r[k] = cplxIn[i + myPosSrc + l].real();
                    dataBuffer[myPosDst + l].i[k] = cplxIn[i + myPosSrc + l].imag();
                }
                for (size_t l = 0; l < myNegSize; ++l) {
                    dataBuffer[myNegDst + l].r[k] = cplxIn[i + myNegSrc + l].real();
                    dataBuffer[myNegDst + l].i[k] = cplxIn[i + myNegSrc + l].imag();
                }
                i += myNbCarriers;
            }
            else {
                for (size_t l = 0; l < myNbCarriers; ++l) {
                    dataBuffer[l].r[k] = 0.0f;
                    dataBuffer[l].i[k] = 0.0f;
                }
            }
        }

        kiss_fft(myFftPlan, myFftBuffer, myFftBuffer);

        for (int k = 0; k < 4; ++k) {
            if (j < sizeOut) {
                for (size_t l = 0; l < mySpacing; ++l) {
                    cplxOut[j + l] = complexf(dataBuffer[l].r[k], dataBuffer[l].i[k]);
                }
                j += mySpacing;
            }
        }
    }
#  else
    for (size_t i = 0; i < myNbSymbols; ++i) {
        FFT_REAL(myFftBuffer[0]) = 0;
        FFT_IMAG(myFftBuffer[0]) = 0;
        bzero(&myFftBuffer[myZeroDst], myZeroSize * sizeof(FFT_TYPE));
        memcpy(&myFftBuffer[myPosDst], &in[myPosSrc],
                myPosSize * sizeof(FFT_TYPE));
        memcpy(&myFftBuffer[myNegDst], &in[myNegSrc],
                myNegSize * sizeof(FFT_TYPE));

        kiss_fft(myFftPlan, myFftBuffer, out);

        in += myNbCarriers;
        out += mySpacing;

    }
#  endif
#endif

    return sizeOut;
}

