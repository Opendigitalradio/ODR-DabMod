/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver for the USRP family of devices, and uses the UHD
   library.
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

#ifdef HAVE_OUTPUT_UHD

#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <atomic>
#include <thread>

#include "Log.h"
#include "output/SDR.h"
#include "output/USRPTime.h"
#include "TimestampDecoder.h"
#include "RemoteControl.h"
#include "ThreadsafeQueue.h"

#include <stdio.h>
#include <sys/types.h>

// If the timestamp is further in the future than
// 100 seconds, abort
#define TIMESTAMP_ABORT_FUTURE 100

// Add a delay to increase buffers when
// frames are too far in the future
#define TIMESTAMP_MARGIN_FUTURE 0.5

namespace Output {

class UHD : public Output::SDRDevice
{
    public:
        UHD(SDRDeviceConfig& config);
        UHD(const UHD& other) = delete;
        UHD& operator=(const UHD& other) = delete;
        ~UHD();

        virtual void tune(double lo_offset, double frequency) override;
        virtual double get_tx_freq(void) const override;
        virtual void set_txgain(double txgain) override;
        virtual double get_txgain(void) const override;
        virtual void set_bandwidth(double bandwidth) override;
        virtual double get_bandwidth(void) const override;
        virtual void transmit_frame(const struct FrameData& frame) override;
        virtual RunStatistics get_run_statistics(void) const override;
        virtual double get_real_secs(void) const override;

        virtual void set_rxgain(double rxgain) override;
        virtual double get_rxgain(void) const override;
        virtual size_t receive_frame(
                complexf *buf,
                size_t num_samples,
                struct frame_timestamp& ts,
                double timeout_secs) override;

        // Return true if GPS and reference clock inputs are ok
        virtual bool is_clk_source_ok(void) const override;
        virtual const char* device_name(void) const override;

        virtual double get_temperature(void) const override;

    private:
        SDRDeviceConfig& m_conf;
        uhd::usrp::multi_usrp::sptr m_usrp;
        uhd::tx_streamer::sptr m_tx_stream;
        uhd::rx_streamer::sptr m_rx_stream;
        std::shared_ptr<USRPTime> m_device_time;

        size_t num_underflows = 0;
        size_t num_overflows = 0;
        size_t num_late_packets = 0;
        size_t num_frames_modulated = 0;
        size_t num_underflows_previous = 0;
        size_t num_late_packets_previous = 0;

        // Used to print statistics once a second
        std::chrono::steady_clock::time_point last_print_time;

        // Returns true if we want to verify loss of refclk
        bool refclk_loss_needs_check(void) const;
        mutable bool suppress_refclk_loss_check = false;

        // Poll asynchronous metadata from UHD
        std::atomic<bool> m_running;
        std::thread m_async_rx_thread;
        void stop_threads(void);
        void print_async_thread(void);
};

} // namespace Output

#endif // HAVE_OUTPUT_UHD

