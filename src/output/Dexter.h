/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver using libiio targeting the PrecisionWave DEXTER board.
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

#if defined(HAVE_DEXTER)

#if !defined(HAVE_ZEROMQ)
#error "ZeroMQ is mandatory for DEXTER"
#endif

#include "iio.h"
#include "zmq.hpp"

#include <string>
#include <memory>
#include <ctime>
#include <mutex>
#include <thread>
#include <variant>

#include "output/SDR.h"
#include "ModPlugin.h"
#include "EtiReader.h"
#include "RemoteControl.h"

namespace Output {

enum class DexterClockState {
   Startup,
   Normal,
   Holdover
};

class Dexter : public Output::SDRDevice
{
    public:
        Dexter(SDRDeviceConfig& config);
        Dexter(const Dexter& other) = delete;
        Dexter& operator=(const Dexter& other) = delete;
        virtual ~Dexter();

        virtual void tune(double lo_offset, double frequency) override;
        virtual double get_tx_freq(void) const override;
        virtual void set_txgain(double txgain) override;
        virtual double get_txgain() const override;
        virtual void set_bandwidth(double bandwidth) override;
        virtual double get_bandwidth() const override;
        virtual void transmit_frame(struct FrameData&& frame) override;
        virtual run_statistics_t get_run_statistics() const override;
        virtual double get_real_secs() const override;

        virtual void set_rxgain(double rxgain) override;
        virtual double get_rxgain() const override;
        virtual size_t receive_frame(
                complexf *buf,
                size_t num_samples,
                frame_timestamp& ts,
                double timeout_secs) override;

        // Return true if GPS and reference clock inputs are ok
        virtual bool is_clk_source_ok() override;
        virtual const char* device_name() const override;

        virtual std::optional<double> get_temperature() const override;

    private:
        void channel_up();
        void channel_down();
        void handle_hw_time();

        bool m_channel_is_up = false;

        SDRDeviceConfig& m_conf;

        struct iio_context* m_ctx = nullptr;
        struct iio_device* m_dexter_dsp_tx = nullptr;

        struct iio_device* m_ad9957 = nullptr;
        struct iio_device* m_ad9957_tx0 = nullptr;
        struct iio_channel* m_tx_channel = nullptr;
        struct iio_buffer *m_buffer = nullptr;

        /* Underflows are counted in a separate thread */
        struct iio_context* m_underflow_ctx = nullptr;
        std::atomic<bool> m_running = ATOMIC_VAR_INIT(false);
        std::thread m_underflow_read_thread;
        void underflow_read_process();
        mutable std::mutex m_attr_thread_mutex;
        size_t underflows = 0;

        size_t prev_underflows = 0;
        size_t num_late = 0;
        size_t num_frames_modulated = 0;

        size_t num_buffers_pushed = 0;

        /* Communication with pacontrol */
        zmq::context_t m_zmq_context;
        zmq::socket_t m_zmq_sock;
        std::string m_pacontrol_endpoint;

        /* Clock State */
        DexterClockState m_clock_state = DexterClockState::Startup;

        // Only valid when m_clock_state is not Startup
        uint64_t m_utc_seconds_at_startup = 0;
        uint64_t m_clock_count_at_startup = 0;

        // Only valid when m_clock_state Holdover
        std::chrono::steady_clock::time_point m_holdover_since =
            std::chrono::steady_clock::time_point::min();
        std::time_t m_holdover_since_t = 0;
};

} // namespace Output

#endif //HAVE_DEXTER

