/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   Common interface for all SDR outputs
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

#include <cstdint>
#include <string>
#include <vector>
#include <complex>
#include <optional>

#include "TimestampDecoder.h"

namespace Output {

enum refclk_lock_loss_behaviour_t { CRASH, IGNORE };

using complexf = std::complex<float>;

/* This structure is used as initial configuration for all SDR devices.
 * It must also contain all remote-controllable settings, otherwise
 * they will get lost on a modulator restart. */
struct SDRDeviceConfig {
    std::string device;
    std::string subDevice; // For UHD
    std::string tx_antenna;
    std::string rx_antenna;

    bool fixedPoint = false;

    long masterClockRate = 32768000;
    unsigned sampleRate = 2048000;
    double frequency = 0.0;
    double lo_offset = 0.0;
    double txgain = 0.0;
    double rxgain = 0.0;
    bool enableSync = false;
    double bandwidth = 0.0;
    unsigned upsample = 1;

    // When working with timestamps, mute the frames that
    // do not have a timestamp
    bool muteNoTimestamps = false;
    unsigned dabMode = 0;
    unsigned maxGPSHoldoverTime = 0;

    /* allowed values for UHD      : auto, int, sma, mimo */
    /* allowed values for BladeRF  : pps, 10mhz */
    std::string refclk_src;

    /* allowed values for UHD : int, sma, mimo */
    std::string pps_src;

    /* allowed values for UHD : pos, neg */
    std::string pps_polarity;

    /* What to do when the reference clock PLL loses lock */
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;

    // muting can only be changed using the remote control
    bool muting = false;

    // TCP port on which to serve TX and RX samples for the
    // digital pre distortion learning tool
    uint16_t dpdFeedbackServerPort = 0;
};

// Each frame contains one OFDM frame, and its
// associated timestamp
struct FrameData {
    // Buffer holding frame data
    std::vector<uint8_t> buf;
    size_t sampleSize = sizeof(complexf);

    // A full timestamp contains a TIST according to standard
    // and time information within MNSC with tx_second.
    frame_timestamp ts;
};


// All SDR Devices must implement the SDRDevice interface
class SDRDevice {
    public:
        using run_statistics_t = json::map_t;

        virtual void tune(double lo_offset, double frequency) = 0;
        virtual double get_tx_freq(void) const = 0;
        virtual void set_txgain(double txgain) = 0;
        virtual double get_txgain(void) const = 0;
        virtual void transmit_frame(struct FrameData&& frame) = 0;
        virtual run_statistics_t get_run_statistics(void) const = 0;
        virtual double get_real_secs(void) const = 0;
        virtual void set_rxgain(double rxgain) = 0;
        virtual double get_rxgain(void) const = 0;
        virtual void set_bandwidth(double bandwidth) = 0;
        virtual double get_bandwidth(void) const = 0;
        virtual size_t receive_frame(
                complexf *buf,
                size_t num_samples,
                frame_timestamp& ts,
                double timeout_secs) = 0;

        // Returns device temperature in degrees C
        virtual std::optional<double> get_temperature(void) const = 0;

        // Return true if GPS and reference clock inputs are ok
        virtual bool is_clk_source_ok(void) = 0;

        virtual const char* device_name(void) const = 0;

        virtual void require_timestamp_refresh() { m_require_timestamp_refresh = true; }

    protected:
        bool m_require_timestamp_refresh = false;
};

} // namespace Output
