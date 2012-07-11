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

#include "OfdmGenerator.h"
#include "PcDebug.h"
#include "kiss_fftsimd.h"

#include <stdio.h>
#include <stdexcept>
#include <assert.h>
#include <complex>
typedef std::complex<float> complexf;


OfdmGenerator::OfdmGenerator(size_t nbSymbols,
        size_t nbCarriers,
        size_t spacing,
        bool inverse) :
    ModCodec(ModFormat(myNbSymbols * myNbCarriers * sizeof(FFT_TYPE)),
            ModFormat(myNbSymbols * mySpacing * sizeof(FFT_TYPE))),
    myFftPlan(NULL),
    myFftBuffer(NULL),
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
    } else {
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

    myFftPlan = kiss_fft_alloc(mySpacing, 1, NULL, NULL);
    myFftBuffer = (FFT_TYPE*)memalign(16, mySpacing * sizeof(FFT_TYPE));
}


OfdmGenerator::~OfdmGenerator()
{
    PDEBUG("OfdmGenerator::~OfdmGenerator() @ %p\n", this);

    if (myFftPlan != NULL) {
        kiss_fft_free(myFftPlan);
    }
    if (myFftBuffer != NULL) {
        free(myFftBuffer);
    }
    kiss_fft_cleanup();
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

#ifdef USE_SIMD
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
            } else {
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
                    cplxOut[j + l].real() = dataBuffer[l].r[k];
                    cplxOut[j + l].imag() = dataBuffer[l].i[k];
                }
                j += mySpacing;
            }
        }
    }
#else
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
#endif

    return sizeOut;
}
