/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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
#include "fftw3.h"
#define FFT_TYPE fftwf_complex

#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>
#include <complex>
#include <malloc.h>
typedef std::complex<float> complexf;


OfdmGenerator::OfdmGenerator(size_t nbSymbols,
        size_t nbCarriers,
        size_t spacing,
        bool inverse) :
    ModCodec(ModFormat(nbSymbols * nbCarriers * sizeof(FFT_TYPE)),
            ModFormat(nbSymbols * spacing * sizeof(FFT_TYPE))),
    myFftPlan(NULL),
    myFftIn(NULL), myFftOut(NULL),
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
}


OfdmGenerator::~OfdmGenerator()
{
    PDEBUG("OfdmGenerator::~OfdmGenerator() @ %p\n", this);

    if (myFftIn) {
         fftwf_free(myFftIn);
    }

    if (myFftOut) {
         fftwf_free(myFftOut);
    }

    if (myFftPlan) {
        fftwf_destroy_plan(myFftPlan);
    }
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
    return sizeOut;
}

