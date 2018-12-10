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

#define FFT_TYPE fftwf_complex

#include <string.h>
#include <stdexcept>
#include <assert.h>
#include <string>
#include <numeric>

static const size_t MAX_CLIP_STATS = 10;

OfdmGenerator::OfdmGenerator(size_t nbSymbols,
                             size_t nbCarriers,
                             size_t spacing,
                             bool& enableCfr,
                             float& cfrClip,
                             float& cfrErrorClip,
                             bool inverse) :
    ModCodec(), RemoteControllable("ofdm"),
    myFftPlan(nullptr),
    myFftIn(nullptr), myFftOut(nullptr),
    myNbSymbols(nbSymbols),
    myNbCarriers(nbCarriers),
    mySpacing(spacing),
    myCfr(enableCfr),
    myCfrClip(cfrClip),
    myCfrErrorClip(cfrErrorClip),
    myCfrFft(nullptr),
    // Initialise the PAPRStats to a few seconds worth of samples
    myPaprBeforeCFR(nbSymbols * 50),
    myPaprAfterCFR(nbSymbols * 50)
{
    PDEBUG("OfdmGenerator::OfdmGenerator(%zu, %zu, %zu, %s) @ %p\n",
            nbSymbols, nbCarriers, spacing, inverse ? "true" : "false", this);

    if (nbCarriers > spacing) {
        throw std::runtime_error(
                "OfdmGenerator::OfdmGenerator nbCarriers > spacing!");
    }

    /* register the parameters that can be remote controlled */
    RC_ADD_PARAMETER(cfr, "Enable crest factor reduction");
    RC_ADD_PARAMETER(clip, "CFR: Clip to amplitude");
    RC_ADD_PARAMETER(errorclip, "CFR: Limit error");
    RC_ADD_PARAMETER(clip_stats, "CFR: statistics (clip ratio, errorclip ratio)");
    RC_ADD_PARAMETER(papr, "PAPR measurements (before CFR, after CFR)");

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

    myCfrPostClip = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * N);
    myCfrPostFft = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * N);
    myCfrFft = fftwf_plan_dft_1d(N,
            myCfrPostClip, myCfrPostFft,
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

    if (myCfrPostClip) {
        fftwf_free(myCfrPostClip);
    }

    if (myCfrPostFft) {
        fftwf_free(myCfrPostFft);
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

    // It is not guaranteed that fftw keeps the FFT input vector intact.
    // That's why we copy it to the reference.
    std::vector<complexf> reference;

    // IFFT output before CFR applied, for MER calc
    std::vector<complexf> before_cfr;

    size_t num_clip = 0;
    size_t num_error_clip = 0;

    // For performance reasons, do not calculate MER for every symbol.
    myMERCalcIndex = (myMERCalcIndex + 1) % myNbSymbols;

    // The PAPRStats' clear() is not threadsafe, do not access it
    // from the RC functions.
    if (myPaprClearRequest.exchange(false)) {
        myPaprBeforeCFR.clear();
        myPaprAfterCFR.clear();
    }

    for (size_t i = 0; i < myNbSymbols; ++i) {
        myFftIn[0][0] = 0;
        myFftIn[0][1] = 0;

        /* For TM I this is:
         * ZeroDst=769 ZeroSize=511
         * PosSrc=0 PosDst=1 PosSize=768
         * NegSrc=768 NegDst=1280 NegSize=768
         */
        memset(&myFftIn[myZeroDst], 0, myZeroSize * sizeof(FFT_TYPE));
        memcpy(&myFftIn[myPosDst], &in[myPosSrc],
                myPosSize * sizeof(FFT_TYPE));
        memcpy(&myFftIn[myNegDst], &in[myNegSrc],
                myNegSize * sizeof(FFT_TYPE));


        if (myCfr) {
            reference.resize(mySpacing);
            memcpy(reinterpret_cast<fftwf_complex*>(reference.data()),
                    myFftIn, mySpacing * sizeof(FFT_TYPE));
        }

        fftwf_execute(myFftPlan); // IFFT from myFftIn to myFftOut

        
        if (myCfr) {
            complexf *symbol = reinterpret_cast<complexf*>(myFftOut);
            myPaprBeforeCFR.process_block(symbol, mySpacing);

            if (myMERCalcIndex == i) {
                before_cfr.resize(mySpacing);
                memcpy(reinterpret_cast<fftwf_complex*>(before_cfr.data()),
                        myFftOut, mySpacing * sizeof(FFT_TYPE));
            }

            /* cfr_one_iteration runs the myFftPlan again at the end, and
             * therefore writes the output data to myFftOut.
             */
            const auto stat = cfr_one_iteration(symbol, reference.data());

            // i == 0 always zero power, so the MER ends up being NaN
            if (i > 0) {
                myPaprAfterCFR.process_block(symbol, mySpacing);
            }

            if (i > 0 and myMERCalcIndex == i) {
                /* MER definition, ETSI ETR 290, Annex C
                 *
                 *                       \sum I^2 + Q^2
                 * MER[dB] = 10 log_10( ---------------- )
                 *                      \sum dI^2 + dQ^2
                 * Where I and Q are the ideal coordinates, and dI and dQ are
                 * the errors in the received datapoints.
                 *
                 * In our case, we consider the constellation points given to the
                 * OfdmGenerator as "ideal", and we compare the CFR output to it.
                 */
                double sum_iq = 0;
                double sum_delta = 0;
                for (size_t j = 0; j < mySpacing; j++) {
                    sum_iq += (double)std::norm(before_cfr[j]);
                    sum_delta += (double)std::norm(symbol[j] - before_cfr[j]);
                }

                // Clamp to 90dB, otherwise the MER average is going to be inf
                const double mer = sum_delta > 0 ?
                    10.0 * std::log10(sum_iq / sum_delta) : 90;
                myMERs.push_back(mer);
            }

            num_clip += stat.clip_count;
            num_error_clip += stat.errclip_count;
        }

        memcpy(out, myFftOut, mySpacing * sizeof(FFT_TYPE));

        in += myNbCarriers;
        out += mySpacing;
    }

    if (myCfr) {
        std::lock_guard<std::mutex> lock(myCfrRcMutex);

        const double num_samps = myNbSymbols * mySpacing;
        const double clip_ratio = (double)num_clip / num_samps;

        myClipRatios.push_back(clip_ratio);
        while (myClipRatios.size() > MAX_CLIP_STATS) {
            myClipRatios.pop_front();
        }

        const double errclip_ratio = (double)num_error_clip / num_samps;
        myErrorClipRatios.push_back(errclip_ratio);
        while (myErrorClipRatios.size() > MAX_CLIP_STATS) {
            myErrorClipRatios.pop_front();
        }

        while (myMERs.size() > MAX_CLIP_STATS) {
            myMERs.pop_front();
        }
    }

    return sizeOut;
}

OfdmGenerator::cfr_iter_stat_t OfdmGenerator::cfr_one_iteration(
        complexf *symbol, const complexf *reference)
{
    // use std::norm instead of std::abs to avoid calculating the
    // square roots
    const float clip_squared = myCfrClip * myCfrClip;

    OfdmGenerator::cfr_iter_stat_t ret;

    // Clip
    for (size_t i = 0; i < mySpacing; i++) {
        const float mag_squared = std::norm(symbol[i]);
        if (mag_squared > clip_squared) {
            // normalise absolute value to myCfrClip:
            // x_clipped = x * clip / |x|
            //           = x * sqrt(clip_squared) / sqrt(mag_squared)
            //           = x * sqrt(clip_squared / mag_squared)
            symbol[i] *= std::sqrt(clip_squared / mag_squared);
            ret.clip_count++;
        }
    }

    // Take FFT of our clipped signal
    memcpy(myCfrPostClip, symbol, mySpacing * sizeof(FFT_TYPE));
    fftwf_execute(myCfrFft); // FFT from myCfrPostClip to myCfrPostFft

    // Calculate the error in frequency domain by subtracting our reference
    // and clip it to myCfrErrorClip. By adding this clipped error signal
    // to our FFT output, we compensate the introduced error to some
    // extent.
    const float err_clip_squared = myCfrErrorClip * myCfrErrorClip;

    std::vector<float> error_norm(mySpacing);

    for (size_t i = 0; i < mySpacing; i++) {
        // FFTW computes an unnormalised transform, i.e. a FFT-IFFT pair
        // or vice-versa gives back the original vector scaled by a factor
        // FFT-size. Because we're comparing our constellation point
        // (calculated with IFFT-clip-FFT) against reference (input to
        // the IFFT), we need to divide by our FFT size.
        const complexf constellation_point =
            reinterpret_cast<complexf*>(myCfrPostFft)[i] / (float)mySpacing;

        complexf error = reference[i] - constellation_point;

        const float mag_squared = std::norm(error);
        error_norm[i] = mag_squared;

        if (mag_squared > err_clip_squared) {
            error *= std::sqrt(err_clip_squared / mag_squared);
            ret.errclip_count++;
        }

        // Update the input to the FFT directly to avoid another copy for the
        // subsequence IFFT
        complexf *fft_in = reinterpret_cast<complexf*>(myFftIn);
        fft_in[i] = constellation_point + error;
    }

    // Run our error-compensated symbol through the IFFT again
    fftwf_execute(myFftPlan); // IFFT from myFftIn to myFftOut

    return ret;
}


void OfdmGenerator::set_parameter(const std::string& parameter,
                                  const std::string& value)
{
    using namespace std;
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "cfr") {
        ss >> myCfr;
        myPaprClearRequest.store(true);
    }
    else if (parameter == "clip") {
        ss >> myCfrClip;
        myPaprClearRequest.store(true);
    }
    else if (parameter == "errorclip") {
        ss >> myCfrErrorClip;
        myPaprClearRequest.store(true);
    }
    else if (parameter == "clip_stats" or parameter == "papr") {
        throw ParameterError("Parameter '" + parameter + "' is read-only");
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
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
    else if (parameter == "clip_stats") {
        std::lock_guard<std::mutex> lock(myCfrRcMutex);
        if (myClipRatios.empty() or myErrorClipRatios.empty() or myMERs.empty()) {
            ss << "No stats available";
        }
        else {
            const double avg_clip_ratio =
                std::accumulate(myClipRatios.begin(), myClipRatios.end(), 0.0) /
                myClipRatios.size();

            const double avg_errclip_ratio =
                std::accumulate(myErrorClipRatios.begin(), myErrorClipRatios.end(), 0.0) /
                myErrorClipRatios.size();

            const double avg_mer =
                std::accumulate(myMERs.begin(), myMERs.end(), 0.0) /
                myMERs.size();

            ss << "Statistics : " << std::fixed <<
                avg_clip_ratio * 100 << "%"" samples clipped, " <<
                avg_errclip_ratio * 100 << "%"" errors clipped. " <<
                "MER after CFR: " << avg_mer << " dB";
        }
    }
    else if (parameter == "papr") {
        const double papr_before = myPaprBeforeCFR.calculate_papr();
        const double papr_after = myPaprAfterCFR.calculate_papr();

        ss << "PAPR [dB]: " << std::fixed <<
            (papr_before == 0 ? string("N/A") : to_string(papr_before)) <<
            ", " <<
            (papr_after == 0 ? string("N/A") : to_string(papr_after));
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}
