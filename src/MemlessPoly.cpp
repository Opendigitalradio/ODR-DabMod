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

// Number of AM/AM coefs, identical to number of AM/PM coefs
#define NUM_COEFS 5

MemlessPoly::MemlessPoly(const std::string& coefs_file, unsigned int num_threads) :
    PipelinedModCodec(),
    RemoteControllable("memlesspoly"),
    m_coefs_am(),
    m_coefs_pm(),
    m_coefs_file(coefs_file),
    m_coefs_mutex()
{
    PDEBUG("MemlessPoly::MemlessPoly(%s) @ %p\n",
            coefs_file.c_str(), this);

    RC_ADD_PARAMETER(ncoefs, "(Read-only) number of coefficients.");
    RC_ADD_PARAMETER(coeffile, "Filename containing coefficients. "
            "When set, the file gets loaded.");

    if (num_threads == 0) {
        const unsigned int hw_concurrency = std::thread::hardware_concurrency();
        etiLog.level(info) << "Polynomial Predistorter will use " <<
            hw_concurrency << " threads (auto detected)";

        for (size_t i = 0; i < hw_concurrency; i++) {
            m_workers.emplace_back();
        }

        for (auto& worker : m_workers) {
            worker.thread = std::thread(
                    &MemlessPoly::worker_thread, &worker);
        }
    }
    else {
        etiLog.level(info) << "Polynomial Predistorter will use " <<
            num_threads << " threads (set in config file)";

        for (size_t i = 0; i < num_threads; i++) {
            m_workers.emplace_back();
        }

        for (auto& worker : m_workers) {
            worker.thread = std::thread(
                    &MemlessPoly::worker_thread, &worker);
        }
    }

    load_coefficients(m_coefs_file);

    start_pipeline_thread();
}

void MemlessPoly::load_coefficients(const std::string &coefFile)
{
    std::vector<float> coefs_am;
    std::vector<float> coefs_pm;
    std::ifstream coef_fstream(coefFile.c_str());
    if (!coef_fstream) {
        throw std::runtime_error("MemlessPoly: Could not open file with coefs!");
    }
    int n_coefs;
    coef_fstream >> n_coefs;

    if (n_coefs <= 0) {
        throw std::runtime_error("MemlessPoly: coefs file has invalid format.");
    }
    else if (n_coefs != NUM_COEFS) {
        throw std::runtime_error("MemlessPoly: invalid number of coefs: " +
                std::to_string(n_coefs) + " expected " + std::to_string(NUM_COEFS));
    }

    const int n_entries = 2 * n_coefs;

    etiLog.log(debug, "MemlessPoly: Reading %d coefs...", n_entries);

    coefs_am.resize(n_coefs);
    coefs_pm.resize(n_coefs);

    for (int n = 0; n < n_entries; n++) {
        float a;
        coef_fstream >> a;

        if (n < n_coefs) {
            coefs_am[n] = a;
        }
        else {
            coefs_pm[n - n_coefs] = a;
        }

        if (coef_fstream.eof()) {
            etiLog.log(error, "MemlessPoly: file %s should contains %d coefs, "
                    "but EOF reached after %d coefs !",
                    coefFile.c_str(), n_entries, n);
            throw std::runtime_error("MemlessPoly: coefs file invalid !");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_coefs_mutex);

        m_coefs_am = coefs_am;
        m_coefs_pm = coefs_pm;
    }
}

/* The restrict keyword is C99, g++ and clang++ however support __restrict
 * instead, and this allows the compiler to auto-vectorize the loop.
 */
static void apply_coeff(
        const float *__restrict coefs_am, const float *__restrict coefs_pm,
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

void MemlessPoly::worker_thread(MemlessPoly::worker_t *workerdata)
{
    while (true) {
        worker_t::input_data_t in_data;
        workerdata->in_queue.wait_and_pop(in_data);

        if (in_data.terminate) {
            break;
        }

        apply_coeff(in_data.coefs_am, in_data.coefs_pm,
                in_data.in, in_data.start, in_data.stop,
                in_data.out);

        workerdata->out_queue.push(1);
    }
}

int MemlessPoly::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
    dataOut->setLength(dataIn->getLength());

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    {
        std::lock_guard<std::mutex> lock(m_coefs_mutex);
        const size_t num_threads = m_workers.size();

        if (num_threads > 0) {
            const size_t step = sizeOut / num_threads;

            size_t start = 0;
            for (auto& worker : m_workers) {
                worker_t::input_data_t dat;
                dat.terminate = false;
                dat.coefs_am = m_coefs_am.data();
                dat.coefs_pm = m_coefs_pm.data();
                dat.in = in;
                dat.start = start;
                dat.stop = start + step;
                dat.out = out;

                worker.in_queue.push(dat);

                start += step;
            }

            // Do the last in this thread
            apply_coeff(m_coefs_am.data(), m_coefs_pm.data(),
                    in, start, sizeOut, out);

            // Wait for completion of the tasks
            for (auto& worker : m_workers) {
                int ret;
                worker.out_queue.wait_and_pop(ret);
            }
        }
        else {
            apply_coeff(m_coefs_am.data(), m_coefs_pm.data(),
                    in, 0, sizeOut, out);
        }
    }

    return dataOut->getLength();
}

void MemlessPoly::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "ncoefs") {
        throw ParameterError("Parameter 'ncoefs' is read-only");
    }
    else if (parameter == "coeffile") {
        try {
            load_coefficients(value);
            m_coefs_file = value;
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
    if (parameter == "ncoefs") {
        ss << m_coefs_am.size();
    }
    else if (parameter == "coeffile") {
        ss << m_coefs_file;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

