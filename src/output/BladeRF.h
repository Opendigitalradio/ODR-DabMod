/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver for the BladeRF family of devices, and uses the libbladerf
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

 //#ifdef HAVE_OUTPUT_BLADERF

 //#include <uhd/utils/safe_main.hpp>
 //#include <uhd/usrp/multi_usrp.hpp>
 //#include <chrono>
 #include <memory>
 #include <string>
 #include <atomic>
 #include <thread>

 #include "Log.h"
 #include "output/SDR.h"
 //#include "output/USRPTime.h"
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

class BladeRF : public Output::SDRDevice
{
   public:
       libbladerf(SDRDeviceConfig& config);
       libbladerf(const libbladerf& other) = delete;
       libbladerf& operator=(const libbladerf& other) = delete;
       ~libbladerf();

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
       // https://nuand.com/bladeRF-doc/libbladeRF/v2.2.1/structbladerf__devinfo.html#a4369c00791073f53ce0d4c606df27c6f
       struct bladerf *m_device;
       bladerf_channel m_channel = BLADERF_CHANNEL_TX(0); // ..._TX(1) is possible too
       bool m_tx_stream_active = false;
       size_t m_interpolate = 1;
       std::vector<complexf> interpolatebuf;
       std::vector<short> m_i16samples;
       std::atomic<float> m_last_fifo_fill_percent = ATOMIC_VAR_INIT(0);

       size_t num_underflows = 0;
       size_t num_overflows = 0;
       size_t num_late_packets = 0;
       size_t num_frames_modulated = 0;
       //size_t num_underflows_previous = 0;
       //size_t num_late_packets_previous = 0;
};

} // namespace Output

#endif // HAVE_OUTPUT_LIBBLADERF