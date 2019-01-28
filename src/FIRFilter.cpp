/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

   This block implements a FIR filter. The real filter taps are given
   as floats, and the block can take advantage of SSE.
   For better performance, filtering is done in another thread, leading
   to a pipeline delay of two calls to FIRFilter::process
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

#include "FIRFilter.h"
#include "PcDebug.h"
#include "Utils.h"

#include <stdio.h>
#include <stdexcept>

#include <array>
#include <iostream>
#include <fstream>
#include <memory>

#ifdef __SSE__
#    include <xmmintrin.h>
#endif

using namespace std;

/* This is the FIR Filter calculated with the doc/fir-filter/generate-filter.py script
 * with settings
 *   gain = 1
 *   sampling_freq = 2.048e6
 *   cutoff = 810e3
 *   transition_width = 250e3
 *
 * It is a good default filter for the common scenarios.
 */
static const std::array<float, 45> default_filter_taps({
        -0.00110450468492, 0.00120703084394, -0.000840645749122, -0.000187368263141,
        0.00184351124335, -0.00355578539893, 0.00419321097434, -0.00254214904271,
        -0.00183473504148, 0.00781436730176, -0.0125957569107, 0.0126200336963,
        -0.00537294941023, -0.00866683479398, 0.0249746385962, -0.0356550291181,
        0.0319730602205, -0.00795613788068, -0.0363943465054, 0.0938014090061,
        -0.151176810265, 0.193567320704, 0.791776955128, 0.193567320704,
        -0.151176810265, 0.0938014090061, -0.0363943465054, -0.00795613788068,
        0.0319730602205, -0.0356550291181, 0.0249746385962, -0.00866683479398,
        -0.00537294941023, 0.0126200336963, -0.0125957569107, 0.00781436730176,
        -0.00183473504148, -0.00254214904271, 0.00419321097434, -0.00355578539893,
        0.00184351124335, -0.000187368263141, -0.000840645749122, 0.00120703084394,
        -0.00110450468492});


FIRFilter::FIRFilter(std::string& taps_file) :
    PipelinedModCodec(),
    RemoteControllable("firfilter"),
    m_taps_file(taps_file)
{
    PDEBUG("FIRFilter::FIRFilter(%s) @ %p\n",
            taps_file.c_str(), this);

    RC_ADD_PARAMETER(ntaps, "(Read-only) number of filter taps.");
    RC_ADD_PARAMETER(tapsfile, "Filename containing filter taps. When written to, the new file gets automatically loaded.");

    load_filter_taps(m_taps_file);

    start_pipeline_thread();
}

FIRFilter::~FIRFilter()
{
    stop_pipeline_thread();
}

void FIRFilter::load_filter_taps(const std::string &tapsFile)
{
    std::vector<float> filter_taps;
    if (tapsFile == "default") {
        std::copy(default_filter_taps.begin(), default_filter_taps.end(),
                std::back_inserter(filter_taps));
    }
    else {
        std::ifstream taps_fstream(tapsFile.c_str());
        if (!taps_fstream) {
            throw std::runtime_error("FIRFilter: Could not open taps file " + tapsFile);
        }
        int n_taps;
        taps_fstream >> n_taps;

        if (n_taps <= 0) {
            throw std::runtime_error("FIRFilter: taps file has invalid format.");
        }

        if (n_taps > 100) {
            etiLog.level(warn) << "FIRFilter: warning: taps file has more than 100 taps";
        }

        etiLog.level(debug) << "FIRFilter: Reading " << n_taps << " taps...";

        filter_taps.resize(n_taps);

        int n;
        for (n = 0; n < n_taps; n++) {
            taps_fstream >> filter_taps[n];
            PDEBUG("FIRFilter: tap: %f\n",  (double)filter_taps[n] );
            if (taps_fstream.eof()) {
                throw std::runtime_error(
                        "FIRFilter: file " + tapsFile +
                        " should contain " + to_string(n_taps) +
                        " taps, but EOF reached after " + to_string(n) +
                        " taps!");
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_taps_mutex);

        m_taps = filter_taps;
    }
}


int FIRFilter::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
        size_t i;

#if __SSE__
        // The SSE accelerated version cannot work on the complex values,
        // it is necessary to do the convolution on the real and imaginary
        // parts separately. Thankfully, the taps are real, simplifying the
        // procedure.

        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        if ((uintptr_t)(&out[0]) % 16 != 0) {
            throw std::runtime_error("FIRFilterWorker: out not aligned");
        }

        __m128 SSEout;
        __m128 SSEtaps;
        __m128 SSEin;
        {
            std::lock_guard<std::mutex> lock(m_taps_mutex);

            for (i = 0; i < sizeIn - 2*m_taps.size(); i += 4) {
                SSEout = _mm_setr_ps(0,0,0,0);

                for (size_t j = 0; j < m_taps.size(); j++) {
                    if ((uintptr_t)(&in[i+2*j]) % 16 == 0) {
                        SSEin = _mm_load_ps(&in[i+2*j]); //faster when aligned
                    }
                    else {
                        SSEin = _mm_loadu_ps(&in[i+2*j]);
                    }

                    SSEtaps = _mm_load1_ps(&m_taps[j]);

                    SSEout = _mm_add_ps(SSEout, _mm_mul_ps(SSEin, SSEtaps));
                }
                _mm_store_ps(&out[i], SSEout);
            }

            for (; i < sizeIn; i++) {
                out[i] = 0.0;
                for (int j = 0; i+2*j < sizeIn; j++) {
                    out[i] += in[i+2*j] * m_taps[j];
                }
            }
        }

#else
        // No SSE ? Loop unrolling should make this faster. As for the SSE,
        // the real and imaginary parts are calculated separately.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        {
            std::lock_guard<std::mutex> lock(m_taps_mutex);
            // Convolve by aligning both frame and taps at zero.
            for (i = 0; i < sizeIn - 2*m_taps.size(); i += 4) {
                out[i]    = 0.0;
                out[i+1]  = 0.0;
                out[i+2]  = 0.0;
                out[i+3]  = 0.0;

                for (size_t j = 0; j < m_taps.size(); j++) {
                    out[i]   += in[i   + 2*j] * m_taps[j];
                    out[i+1] += in[i+1 + 2*j] * m_taps[j];
                    out[i+2] += in[i+2 + 2*j] * m_taps[j];
                    out[i+3] += in[i+3 + 2*j] * m_taps[j];
                }
            }

            // At the end of the frame, we cut the convolution off.
            // The beginning of the next frame starts with a NULL symbol
            // anyway.
            for (; i < sizeIn; i++) {
                out[i] = 0.0;
                for (int j = 0; i+2*j < sizeIn; j++) {
                    out[i] += in[i+2*j] * m_taps[j];
                }
            }
        }
#endif

        // The following implementations are for debugging only.
#if 0
        // Same thing as above, without loop unrolling. For debugging.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        std::lock_guard<std::mutex> lock(m_taps_mutex);

        for (i = 0; i < sizeIn - 2*m_taps.size(); i += 1) {
            out[i]  = 0.0;

            for (size_t j = 0; j < m_taps.size(); j++) {
                out[i]  += in[i+2*j] * m_taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; i+2*j < sizeIn; j++) {
                out[i] += in[i+2*j] * m_taps[j];
            }
        }

#elif 0
        // An unrolled loop, but this time, the input data is cast to complex float.
        // Makes indices more natural. For debugging.
        const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
        complexf* out      = reinterpret_cast<complexf*>(dataOut->getData());
        size_t sizeIn      = dataIn->getLength() / sizeof(complexf);

        std::lock_guard<std::mutex> lock(m_taps_mutex);

        for (i = 0; i < sizeIn - m_taps.size(); i += 4) {
            out[i]   = 0.0;
            out[i+1] = 0.0;
            out[i+2] = 0.0;
            out[i+3] = 0.0;

            for (size_t j = 0; j < m_taps.size(); j++) {
                out[i]   += in[i+j  ] * m_taps[j];
                out[i+1] += in[i+1+j] * m_taps[j];
                out[i+2] += in[i+2+j] * m_taps[j];
                out[i+3] += in[i+3+j] * m_taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; j+i < sizeIn; j++) {
                out[i] += in[i+j] * m_taps[j];
            }
        }

#elif 0
        // Simple implementation. Slow. For debugging.
        const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
        complexf* out      = reinterpret_cast<complexf*>(dataOut->getData());
        size_t sizeIn      = dataIn->getLength() / sizeof(complexf);

        std::lock_guard<std::mutex> lock(m_taps_mutex);

        for (i = 0; i < sizeIn - m_taps.size(); i += 1) {
            out[i]   = 0.0;

            for (size_t j = 0; j < m_taps.size(); j++) {
                out[i]  += in[i+j  ] * m_taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; j+i < sizeIn; j++) {
                out[i] += in[i+j] * m_taps[j];
            }
        }
#endif

    return dataOut->getLength();
}

void FIRFilter::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "ntaps") {
        throw ParameterError("Parameter 'ntaps' is read-only");
    }
    else if (parameter == "tapsfile") {
        try {
            load_filter_taps(value);
            m_taps_file = value;
        }
        catch (const std::runtime_error &e) {
            throw ParameterError(e.what());
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string FIRFilter::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "ntaps") {
        ss << m_taps.size();
    }
    else if (parameter == "tapsfile") {
        ss << m_taps_file;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

