/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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
#include <string>


OfdmGenerator::OfdmGenerator(size_t nbSymbols,
        size_t nbCarriers,
        size_t spacing,
        bool inverse) :
    ModCodec(), RemoteControllable("ofdm"),
    myFftPlan(nullptr),
    myFftIn(nullptr), myFftOut(nullptr),
    myNbSymbols(nbSymbols),
    myNbCarriers(nbCarriers),
    mySpacing(spacing),
    myCfr(false),
    myCfrClip(1.0f),
    myCfrErrorClip(1.0f),
    myCfrFft(nullptr)
{
    PDEBUG("OfdmGenerator::OfdmGenerator(%zu, %zu, %zu, %s) @ %p\n",
            nbSymbols, nbCarriers, spacing, inverse ? "true" : "false", this);

    if (nbCarriers > spacing) {
        throw std::runtime_error(
                "OfdmGenerator::OfdmGenerator nbCarriers > spacing!");
    }

    /* register the parameters that can be remote controlled */
    RC_ADD_PARAMETER(cfr, "Enable crest factor reduction");
    RC_ADD_PARAMETER(cfrclip, "CFR: Clip to amplitude");
    RC_ADD_PARAMETER(cfrerrorclip, "CFR: Limit error");

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

    myCfrPostClip.resize(N);
    myCfrPostFft.resize(N);
    myCfrFft = fftwf_plan_dft_1d(N,
            reinterpret_cast<FFT_TYPE*>(myCfrPostClip.data()),
            reinterpret_cast<FFT_TYPE*>(myCfrPostFft.data()),
            FFTW_FORWARD, FFTW_MEASURE);

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

    if (myCfrFft) {
        fftwf_destroy_plan(myCfrFft);
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

        fftwf_execute(myFftPlan); // IFFT from myFftIn to myFftOut

        if (myCfr) {
            cfr_one_iteration(dataOut, i);
        }

        memcpy(out, myFftOut, mySpacing * sizeof(FFT_TYPE));

        in += myNbCarriers;
        out += mySpacing;
    }
    return sizeOut;
}

void OfdmGenerator::cfr_one_iteration(Buffer *symbols, size_t symbol_ix)
{
    complexf* out = reinterpret_cast<complexf*>(symbols->getData());

    out += (symbol_ix * mySpacing);

    // use std::norm instead of std::abs to avoid calculating the
    // square roots
    const float clip_squared = myCfrClip * myCfrClip;

    // Clip
    for (size_t i = 0; i < mySpacing; i++) {
        const float mag_squared = std::norm(out[i]);
        if (mag_squared > clip_squared) {
            // normalise absolute value to myCfrClip:
            // x_clipped = x * clip / |x|
            //           = x * sqrt(clip_squared) / sqrt(mag_squared)
            //           = x * sqrt(clip_squared / mag_squared)
            out[i] *= std::sqrt(clip_squared / mag_squared);
        }
    }

    // Take FFT of our clipped signal
    std::copy(out, out + mySpacing, myCfrPostClip.begin());
    fftwf_execute(myCfrFft); // FFT from myCfrPostClip to myCfrPostFft

    // Calculate the error in frequency domain by subtracting our reference
    // and clip it to myCfrErrorClip. By adding this clipped error signal
    // to our FFT output, we compensate the introduced error to some
    // extent.
    const float err_clip_squared = myCfrErrorClip * myCfrErrorClip;

    complexf *reference = reinterpret_cast<complexf*>(myFftIn);
    for (size_t i = 0; i < mySpacing; i++) {
        complexf error = myCfrPostFft[i] - reference[i];

        const float mag_squared = std::norm(error);
        if (mag_squared > err_clip_squared) {
            error *= std::sqrt(err_clip_squared / mag_squared);
        }

        // Update the reference in-place to avoid another copy for the
        // subsequence IFFT
        reference[i] += error;
    }

    // Run our error-compensated symbol through the IFFT again
    fftwf_execute(myFftPlan); // IFFT from myFftIn to myFftOut

}


void OfdmGenerator::set_parameter(const std::string& parameter,
                                  const std::string& value)
{
    using namespace std;
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "cfr") {
        ss >> myCfr;
    }
    else if (parameter == "clip") {
        ss >> myCfrClip;
    }
    else if (parameter == "errorclip") {
        ss >> myCfrErrorClip;
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const std::string OfdmGenerator::get_parameter(const std::string& parameter) const
{
    using namespace std;
    stringstream ss;
    if (parameter == "cfr") {
        ss << myCfr;
    }
    else if (parameter == "clip") {
        ss << std::fixed << myCfrClip;
    }
    else if (parameter == "errorclip") {
        ss << std::fixed << myCfrErrorClip;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}
