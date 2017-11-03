/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "ModPlugin.h"
#include "EtiReader.h"

namespace Output {

using complexf = std::complex<float>;

enum refclk_lock_loss_behaviour_t { CRASH, IGNORE };

/* This structure is used as initial configuration for all SDR devices.
 * It must also contain all remote-controllable settings, otherwise
 * they will get lost on a modulator restart. */
struct SDRDeviceConfig {
    std::string device;
    std::string subDevice; // For UHD

    long masterClockRate = 32768000;
    unsigned sampleRate = 2048000;
    double frequency = 0.0;
    double lo_offset = 0.0;
    double txgain = 0.0;
    double rxgain = 0.0;
    bool enableSync = false;

    // When working with timestamps, mute the frames that
    // do not have a timestamp
    bool muteNoTimestamps = false;
    unsigned dabMode = 0;
    unsigned maxGPSHoldoverTime = 0;

    /* allowed values for UHD : auto, int, sma, mimo */
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

    // A full timestamp contains a TIST according to standard
    // and time information within MNSC with tx_second.
    struct frame_timestamp ts;
};


// All SDR Devices must implement the SDRDevice interface
class SDRDevice {
    public:
        struct RunStatistics {
            size_t num_underruns;
            size_t num_late_packets;
            size_t num_overruns;
            size_t num_frames_modulated; //TODO increment
        };

        // TODO make some functions const
        virtual void tune(double lo_offset, double frequency) = 0;
        virtual double get_tx_freq(void) = 0;
        virtual void set_txgain(double txgain) = 0;
        virtual double get_txgain(void) = 0;
        virtual void transmit_frame(const struct FrameData& frame) = 0;
        virtual RunStatistics get_run_statistics(void) = 0;
        virtual double get_real_secs(void) = 0;


        // Return true if GPS and reference clock inputs are ok
        virtual bool is_clk_source_ok(void) = 0;

        virtual const char* device_name(void) = 0;
};

class SDR : public ModOutput, public RemoteControllable {
    public:
        SDR(SDRDeviceConfig& config, std::shared_ptr<SDRDevice> device);
        SDR(const SDR& other) = delete;
        SDR operator=(const SDR& other) = delete;
        ~SDR();

        virtual int process(Buffer *dataIn) override;

        virtual const char* name() override;

        void setETISource(EtiSource *etiSource);

        /*********** REMOTE CONTROL ***************/

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value) override;

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const override;

    private:
        void stop(void);
        void process_thread_entry(void);
        void handle_frame(struct FrameData &frame);

        SDRDeviceConfig& m_config;

        std::atomic<bool> m_running;
        std::thread m_device_thread;
        ThreadsafeQueue<FrameData> m_queue;

        std::shared_ptr<SDRDevice> m_device;
        std::string m_name;

        EtiSource *m_eti_source = nullptr;
        bool     sourceContainsTimestamp = false;
        bool     last_tx_time_initialised = false;
        uint32_t last_tx_second = 0;
        uint32_t last_tx_pps = 0;
};

}

