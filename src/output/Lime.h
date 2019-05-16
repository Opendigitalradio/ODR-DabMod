/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Evariste F5OEO, evaristec@gmail.com

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
  It is an output driver using the LimeSDR library.
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
#include <config.h>
#endif

#ifdef HAVE_LIMESDR

#include <atomic>
#include <string>
#include <memory>

#include "output/SDR.h"
#include "ModPlugin.h"
#include "EtiReader.h"
#include "RemoteControl.h"
#include <lime/LimeSuite.h>

namespace Output
{

class Lime : public Output::SDRDevice
{
  public:
    Lime(SDRDeviceConfig &config);
    Lime(const Lime &other) = delete;
    Lime &operator=(const Lime &other) = delete;
    ~Lime();

    virtual void tune(double lo_offset, double frequency) override;
    virtual double get_tx_freq(void) const override;
    virtual void set_txgain(double txgain) override;
    virtual double get_txgain(void) const override;
    virtual void set_bandwidth(double bandwidth) override;
    virtual double get_bandwidth(void) const override;
    virtual void transmit_frame(const struct FrameData &frame) override;
    virtual RunStatistics get_run_statistics(void) const override;
    virtual double get_real_secs(void) const override;

    virtual void set_rxgain(double rxgain) override;
    virtual double get_rxgain(void) const override;
    virtual size_t receive_frame(
        complexf *buf,
        size_t num_samples,
        struct frame_timestamp &ts,
        double timeout_secs) override;

    // Return true if GPS and reference clock inputs are ok
    virtual bool is_clk_source_ok(void) const override;
    virtual const char *device_name(void) const override;

    virtual double get_temperature(void) const override;
    virtual float get_fifo_fill_percent(void) const;

  private:
    SDRDeviceConfig &m_conf;
    lms_device_t *m_device = nullptr;
    size_t m_channel = 0; // Should be set by config
    lms_stream_t m_tx_stream;
    bool m_tx_stream_active = false;
    size_t m_interpolate = 1;
    std::vector<complexf> interpolatebuf;
    std::vector<short> m_i16samples; 
    std::atomic<float> m_last_fifo_fill_percent = ATOMIC_VAR_INIT(0);
    

    size_t underflows = 0;
    size_t overflows = 0;
    size_t late_packets = 0;
    size_t num_frames_modulated = 0;
};

} // namespace Output

#endif //HAVE_SOAPYSDR
