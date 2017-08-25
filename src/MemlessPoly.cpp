/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li
   Andreas Steger, andreas.steger@digris.ch

    http://opendigitalradio.org

   This block implements a memoryless polynom for digital predistortion.
   For better performance, multiplying is done in another thread, leading
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

#pragma GCC optimize ("O3")

#include "MemlessPoly.h"
#include "PcDebug.h"
#include "Utils.h"

#include <stdio.h>
#include <stdexcept>

#include <future>
#include <array>
#include <iostream>
#include <fstream>
#include <memory>
#include <complex>

using namespace std;

#define NUM_COEFS_AM 5
#define NUM_COEFS_PM 5

MemlessPoly::MemlessPoly(const std::string& coefs_am_file, const std::string& coefs_pm_file, unsigned int num_threads) :
    PipelinedModCodec(),
    RemoteControllable("memlesspoly"),
    m_num_threads(num_threads),
    m_coefs_am(),
    m_coefs_am_file(coefs_am_file),
    m_coefs_am_mutex(),
    m_coefs_pm(),
    m_coefs_pm_file(coefs_pm_file),
    m_coefs_pm_mutex()
{
    PDEBUG("MemlessPoly::MemlessPoly(%s) @ %p\n",
            coefs_am_file.c_str(), this);
    PDEBUG("MemlessPoly::MemlessPoly(%s) @ %p\n",
            coefs_pm_file.c_str(), this);

    if (m_num_threads == 0) {
        const unsigned int hw_concurrency = std::thread::hardware_concurrency();
        etiLog.level(info) << "Polynomial Predistorter will use " <<
            hw_concurrency << " threads (auto detected)";
    }
    else {
        etiLog.level(info) << "Polynomial Predistorter will use " <<
            m_num_threads << " threads (set in config file)";
    }

    RC_ADD_PARAMETER(ncoefs_am, "(Read-only) number of coefficients for amplitude.");
    RC_ADD_PARAMETER(coeffile_am, "Filename containing coefficients for amplitude. When written to, the new file gets automatically loaded.");

    RC_ADD_PARAMETER(ncoefs_pm, "(Read-only) number of coefficients for phase.");
    RC_ADD_PARAMETER(coeffile_pm, "Filename containing coefficients for amplitude. When written to, the new file gets automatically loaded.");

    load_coefficients_am(m_coefs_am_file);
    load_coefficients_pm(m_coefs_pm_file);

    start_pipeline_thread();
}

void MemlessPoly::load_coefficients_am(const std::string &coefFile_am)
{
    std::vector<float> coefs_am;
    std::ifstream coef_fstream_am(coefFile_am.c_str());
    if (!coef_fstream_am) {
        throw std::runtime_error("MemlessPoly: Could not open file with coefs_am!");
    }
    int n_coefs_am;
    coef_fstream_am >> n_coefs_am;

    if (n_coefs_am <= 0) {
        throw std::runtime_error("MemlessPoly: coefs_am file has invalid format.");
    }
    else if (n_coefs_am != NUM_COEFS_AM) {
        throw std::runtime_error("MemlessPoly: invalid number of coefs_am: " +
                std::to_string(n_coefs_am) + " expected " + std::to_string(NUM_COEFS_AM));
    }

    etiLog.log(debug, "MemlessPoly: Reading %d coefs_am...", n_coefs_am);

    coefs_am.resize(n_coefs_am);

    for (int n = 0; n < n_coefs_am; n++) {
        float a;
        coef_fstream_am >> a;
        coefs_am[n] = a;

        if (coef_fstream_am.eof()) {
            etiLog.log(error, "MemlessPoly: file %s should contains %d coefs_am, "
                    "but EOF reached after %d coefs_am !",
                    coefFile_am.c_str(), n_coefs_am, n);
            throw std::runtime_error("MemlessPoly: coefs_am file invalid !");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_coefs_am_mutex);

        m_coefs_am = coefs_am;
    }
}

void MemlessPoly::load_coefficients_pm(const std::string &coefFile_pm)
{
    std::vector<float> coefs_pm;
    std::ifstream coef_fstream_pm(coefFile_pm.c_str());
    if (!coef_fstream_pm) {
        throw std::runtime_error("MemlessPoly: Could not open file with coefs_pm!");
    }
    int n_coefs_pm;
    coef_fstream_pm >> n_coefs_pm;

    if (n_coefs_pm <= 0) {
        throw std::runtime_error("MemlessPoly: coefs_pm file has invalid format.");
    }
    else if (n_coefs_pm != NUM_COEFS_PM) {
        throw std::runtime_error("MemlessPoly: invalid number of coefs_pm: " +
                std::to_string(n_coefs_pm) + " expected " + std::to_string(NUM_COEFS_PM));
    }

    etiLog.log(debug, "MemlessPoly: Reading %d coefs_pm...", n_coefs_pm);

    coefs_pm.resize(n_coefs_pm);

    for (int n = 0; n < n_coefs_pm; n++) {
        float a;
        coef_fstream_pm >> a;
        coefs_pm[n] = a;

        if (coef_fstream_pm.eof()) {
            etiLog.log(error, "MemlessPoly: file %s should contains %d coefs_pm, "
                    "but EOF reached after %d coefs_pm !",
                    coefFile_pm.c_str(), n_coefs_pm, n);
            throw std::runtime_error("MemlessPoly: coefs_pm file invalid !");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_coefs_pm_mutex);

        m_coefs_pm = coefs_pm;
    }
}

/* The restrict keyword is C99, g++ and clang++ however support __restrict
 * instead, and this allows the compiler to auto-vectorize the loop.
 */
static void apply_coeff(
        const vector<float> &coefs_am,
        const vector<float> &coefs_pm,
        const complexf *__restrict in, size_t start, size_t stop,
        complexf *__restrict out)
{
    for (size_t i = start; i < stop; i+=1) {

        float in_mag_sq = in[i].real() * in[i].real() + in[i].imag() * in[i].imag();

        float amplitude_correction =
            ( coefs_am[0] + in_mag_sq *
              ( coefs_am[1] + in_mag_sq *
                ( coefs_am[2] + in_mag_sq *
                  ( coefs_am[3] + in_mag_sq *
                    coefs_am[4]))));

        float phase_correction = -1 *
            ( coefs_pm[0] + in_mag_sq *
              ( coefs_pm[1] + in_mag_sq *
                ( coefs_pm[2] + in_mag_sq *
                  ( coefs_pm[3] + in_mag_sq *
                    coefs_pm[4]))));

        float phase_correction_sq = phase_correction * phase_correction;

        // Approximation for Cosinus 1 - 1/2 x^2 + 1/24 x^4 - 1/720 x^6
        float re = (1.0f - phase_correction_sq *
                ( -0.5f + phase_correction_sq *
                    ( 0.486666f  + phase_correction_sq *
                        ( -0.00138888f))));

        // Approximation for Sinus x + 1/6 x^3 + 1/120 x^5
        float im = phase_correction *
                (1.0f + phase_correction_sq *
                    (0.166666f + phase_correction_sq *
                        (0.00833333f)));

        out[i] = in[i] * amplitude_correction * complex<float>(re, im);
    }
}

int MemlessPoly::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
    dataOut->setLength(dataIn->getLength());

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    {
        std::lock_guard<std::mutex> lock_am(m_coefs_am_mutex);
        std::lock_guard<std::mutex> lock_pm(m_coefs_pm_mutex);
        const unsigned int hw_concurrency = std::thread::hardware_concurrency();

        const unsigned int num_threads =
            (m_num_threads > 0) ? m_num_threads : hw_concurrency;

        if (num_threads) {
            const size_t step = sizeOut / num_threads;
            vector<future<void> > flags;

            size_t start = 0;
            for (size_t i = 0; i < num_threads - 1; i++) {
                flags.push_back(async(launch::async, apply_coeff,
                            m_coefs_am, m_coefs_pm, in, start, start + step, out));

                start += step;
            }

            // Do the last in this thread
            apply_coeff(m_coefs_am, m_coefs_pm, in, start, sizeOut, out);

            // Wait for completion of the tasks
            for (auto& f : flags) {
                f.get();
            }
        }
        else {
            apply_coeff(m_coefs_am, m_coefs_pm, in, 0, sizeOut, out);
        }
    }

    return dataOut->getLength();
}

void MemlessPoly::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "ncoefs_am") {
        throw ParameterError("Parameter 'ncoefs_am' is read-only");
    }
    else if (parameter == "ncoefs_pm") {
        throw ParameterError("Parameter 'ncoefs_pm' is read-only");
    }
    else if (parameter == "coeffile_am") {
        try {
            load_coefficients_am(value);
            m_coefs_am_file = value;
        }
        catch (std::runtime_error &e) {
            throw ParameterError(e.what());
        }
    }
    else if (parameter == "coeffile_pm") {
        try {
            load_coefficients_pm(value);
            m_coefs_pm_file = value;
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
    if (parameter == "ncoefs_am") {
        ss << m_coefs_am.size();
    }
    else if (parameter == "ncoefs_pm") {
        ss << m_coefs_pm.size();
    }
    else if (parameter == "coeffile_am") {
        ss << m_coefs_am_file;
    }
    else if (parameter == "coeffile_pm") {
        ss << m_coefs_pm_file;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

