/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2022
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

#ifdef HAVE_DEXTER
#include "iio.h"

#include <string>
#include <memory>
#include <ctime>

#include "output/SDR.h"
#include "ModPlugin.h"
#include "EtiReader.h"
#include "RemoteControl.h"

namespace Output {

class Dexter : public Output::SDRDevice
{
    public:
        Dexter(SDRDeviceConfig& config);
        Dexter(const Dexter& other) = delete;
        Dexter& operator=(const Dexter& other) = delete;
        ~Dexter();

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
                frame_timestamp& ts,
                double timeout_secs) override;

        // Return true if GPS and reference clock inputs are ok
        virtual bool is_clk_source_ok(void) const override;
        virtual const char* device_name(void) const override;

        virtual double get_temperature(void) const override;

    private:
        SDRDeviceConfig& m_conf;

        struct iio_context* m_ctx = nullptr;
        struct iio_device* m_dexter_dsp_tx = nullptr;

        struct iio_device* m_ad9957_tx0 = nullptr;
        struct iio_channel* m_tx_channel = nullptr;
        struct iio_buffer *m_buffer = nullptr;

        size_t underflows = 0;
        size_t num_late = 0;
        size_t num_frames_modulated = 0;

        uint64_t m_utc_seconds_at_startup;
        uint64_t m_clock_count_at_startup = 0;
        uint64_t m_clock_count_frame = 0;
};

} // namespace Output

#endif //HAVE_DEXTER

