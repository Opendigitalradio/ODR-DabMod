/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "RemoteControl.h"
#include "ModPlugin.h"
#include "PcDebug.h"
#include "ThreadsafeQueue.h"

#include <sys/types.h>
#include <array>
#include <complex>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <ctime>
#include <cstdio>

#define MEMLESSPOLY_PIPELINE_DELAY 1

typedef std::complex<float> complexf;

enum class dpd_type_t {
    odd_only_poly,
    lookup_table
};


class MemlessPoly : public PipelinedModCodec, public RemoteControllable
{
public:
    MemlessPoly(std::string& coefs_file, unsigned int num_threads);
    MemlessPoly(const MemlessPoly& other) = delete;
    MemlessPoly& operator=(const MemlessPoly& other) = delete;
    virtual ~MemlessPoly();

    virtual const char* name() { return "MemlessPoly"; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter,
            const std::string& value);

    virtual const std::string get_parameter(
            const std::string& parameter) const;

private:
    int internal_process(Buffer* const dataIn, Buffer* dataOut);
    void load_coefficients(std::istream& coefData);
    std::string serialise_coefficients() const;

    struct worker_t {
        struct input_data_t {
            dpd_type_t dpd_type;

            // Valid for polynomial types
            const float *coefs_am = nullptr;
            const float *coefs_pm = nullptr;

            // Valid for LUT
            float lut_scalefactor = 0.0f;
            const complexf *lut = nullptr;

            const complexf *in = nullptr;
            size_t start = 0;
            size_t stop = 0;
            complexf *out = nullptr;
        };

        worker_t() {}
        worker_t(const worker_t& other) = delete;
        worker_t operator=(const worker_t& other) = delete;
        worker_t operator=(worker_t&& other) = delete;

        // The move constructor creates a new in_queue and out_queue,
        // because ThreadsafeQueue is neither copy- nor move-constructible.
        // Not an issue because creating the workers happens at startup, before
        // the first work item.
        worker_t(worker_t&& other) :
            in_queue(),
            out_queue(),
            thread(std::move(other.thread)) {}

        ~worker_t() {
            if (thread.joinable()) {
                in_queue.trigger_wakeup();
                thread.join();
            }
        }

        ThreadsafeQueue<input_data_t> in_queue;
        ThreadsafeQueue<int> out_queue;

        std::thread thread;
    };

    std::vector<worker_t> m_workers;

    static void worker_thread(worker_t *workerdata);

    bool m_dpd_settings_valid = false;
    dpd_type_t m_dpd_type;
    std::vector<float> m_coefs_am; // AM/AM coefficients
    std::vector<float> m_coefs_pm; // AM/PM coefficients

    float m_lut_scalefactor; // Scale value applied before looking up in LUT

    static constexpr size_t lut_entries = 32;
    std::array<complexf, lut_entries> m_lut; // Lookup table correction factors

    std::string& m_coefs_file;
    mutable std::mutex m_coefs_mutex;
};

