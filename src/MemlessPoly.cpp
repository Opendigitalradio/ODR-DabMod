/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

   This block implements a FIR filter. The real filter taps are given
   as floats, and the block can take advantage of SSE.
   For better performance, filtering is done in another thread, leading
   to a pipeline delay of two calls to MemlessPoly::process
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

#include "MemlessPoly.h"
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

        //0.8, -0.2, 0.2, 0.25,
static const std::array<float, 8> default_coefficients({
        0.1, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
        });


MemlessPoly::MemlessPoly(const std::string& taps_file) :
    PipelinedModCodec(),
    RemoteControllable("memlesspoly"),
    m_taps_file(taps_file)
{
    PDEBUG("MemlessPoly::MemlessPoly(%s) @ %p\n",
            taps_file.c_str(), this);

    RC_ADD_PARAMETER(ntaps, "(Read-only) number of filter taps.");
    RC_ADD_PARAMETER(coeffile, "Filename containing filter taps. When written to, the new file gets automatically loaded.");

    load_filter_taps(m_taps_file);

    start_pipeline_thread();
}

void MemlessPoly::load_filter_taps(const std::string &tapsFile)
{
    std::vector<float> filter_taps;
    if (tapsFile == "default") {
        std::cout << "MemlessPoly default\n";
        std::copy(default_coefficients.begin(), default_coefficients.end(),
                std::back_inserter(filter_taps));
    }
    else {
        std::ifstream taps_fstream(tapsFile.c_str());
        if(!taps_fstream) {
            fprintf(stderr, "MemlessPoly: file %s could not be opened !\n", tapsFile.c_str());
            throw std::runtime_error("MemlessPoly: Could not open file with taps! ");
        }
        int n_taps;
        taps_fstream >> n_taps;

        if (n_taps <= 0) {
            fprintf(stderr, "MemlessPoly: warning: taps file has invalid format\n");
            throw std::runtime_error("MemlessPoly: taps file has invalid format.");
        }

        if (n_taps > 100) {
            fprintf(stderr, "MemlessPoly: warning: taps file has more than 100 taps\n");
        }

        fprintf(stderr, "MemlessPoly: Reading %d taps...\n", n_taps);

        filter_taps.resize(n_taps);

        int n;
        for (n = 0; n < n_taps; n++) {
            taps_fstream >> filter_taps[n];
            PDEBUG("MemlessPoly: tap: %f\n",  filter_taps[n] );
            if (taps_fstream.eof()) {
                fprintf(stderr, "MemlessPoly: file %s should contains %d taps, but EOF reached "\
                        "after %d taps !\n", tapsFile.c_str(), n_taps, n);
                throw std::runtime_error("MemlessPoly: filtertaps file invalid ! ");
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_taps_mutex);

        m_taps = filter_taps;
    }
}


int MemlessPoly::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
        size_t i;

        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        {
             std::lock_guard<std::mutex> lock(m_taps_mutex);
             for (i = 0; i < sizeIn; i += 1) {
                 float mag = std::abs(in[i]);
                 //out[i] = in[i];
                 out[i] = in[i] * (
                     m_taps[0] +
                     m_taps[1] * mag +
                     m_taps[2] * mag*mag +
                     m_taps[3] * mag*mag*mag +
                     m_taps[4] * mag*mag*mag*mag +
                     m_taps[5] * mag*mag*mag*mag*mag +
                     m_taps[6] * mag*mag*mag*mag*mag*mag +
                     m_taps[7] * mag*mag*mag*mag*mag*mag*mag
                     );
            }
        }

    return dataOut->getLength();
}

void MemlessPoly::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "ntaps") {
        throw ParameterError("Parameter 'ntaps' is read-only");
    }
    else if (parameter == "coeffile") {
        try {
            load_filter_taps(value);
            m_taps_file = value;
        }
        catch (std::runtime_error &e) {
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

const string MemlessPoly::get_parameter(const string& parameter) const
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

