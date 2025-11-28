/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2025
   Evariste F5OEO, evaristec@gmail.com

   http://opendigitalradio.org

DESCRIPTION:
  It is an output driver using the TezukaFirmware for PlutoSDR.
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

#ifdef HAVE_PLUTOTEZUKA

#include "iio.h"

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

namespace Output
{

class PlutoTezuka : public Output::SDRDevice
{
  public:
    PlutoTezuka(SDRDeviceConfig &config);
    PlutoTezuka(const PlutoTezuka &other) = delete;
    PlutoTezuka &operator=(const PlutoTezuka &other) = delete;
    ~PlutoTezuka();
    
    // --- SDRDevice Overrides ---
    virtual void tune(double lo_offset, double frequency) override;
    virtual double get_tx_freq(void) const override;
    virtual void set_txgain(double txgain) override;
    virtual double get_txgain(void) const override;
    virtual void set_bandwidth(double bandwidth) override;
    virtual double get_bandwidth(void) const override;
    virtual void transmit_frame(struct FrameData&& frame) override;
    virtual run_statistics_t get_run_statistics(void) const override;
    virtual double get_real_secs(void) const override;

    virtual void set_rxgain(double rxgain) override;
    virtual double get_rxgain(void) const override;
    virtual size_t receive_frame(
        complexf *buf,
        size_t num_samples,
        frame_timestamp &ts,
        double timeout_secs) override;

    virtual bool is_clk_source_ok(void) override;
    virtual const char *device_name(void) const override;

    virtual std::optional<double> get_temperature(void) const override;

  private:
    SDRDeviceConfig &m_conf;
    
    // **NEW IIO PRIVATE MEMBERS**
    // These pointers manage the state of the PlutoSDR connection via libiio.
    struct iio_context *m_ctx = nullptr;      // IIO Context
    struct iio_device *m_phy_dev = nullptr;   // PHY Device (ad9361-phy) for attributes
    struct iio_device *m_tx_dev = nullptr;    // TX DMA Device (cf-ad9361-dds-core-lpc) for streaming
    struct iio_channel *m_tx0_i = nullptr;    // TX I Channel
    struct iio_channel *m_tx0_q = nullptr;    // TX Q Channel
    struct iio_buffer  *m_tx_buf = nullptr;   // TX Buffer

    size_t m_channel = 0; 
    
    bool m_tx_stream_active = false;
    size_t m_interpolate = 1;
    std::vector<complexf> interpolatebuf;
    std::vector<short> m_i16samples; 
    std::atomic<float> m_last_fifo_fill_percent = ATOMIC_VAR_INIT(0);

    size_t underflows = 0;
    size_t overflows = 0;
    size_t dropped_packets = 0;
    size_t num_frames_modulated = 0;
};

} // namespace Output

#endif