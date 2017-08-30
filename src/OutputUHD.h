/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver for the USRP family of devices, and uses the UHD
   library. This version is multi-threaded. A separate thread sends the data to
   the device.

   Data between the modulator and the UHD thread are exchanged through a
   threadsafe queue.
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

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/thread.hpp>
#include <deque>
#include <chrono>
#include <memory>
#include <string>
#include <atomic>

#include "Log.h"
#include "ModPlugin.h"
#include "EtiReader.h"
#include "TimestampDecoder.h"
#include "RemoteControl.h"
#include "ThreadsafeQueue.h"
#include "OutputUHDFeedback.h"

#include <stdio.h>
#include <sys/types.h>

//#define MDEBUG(fmt, args...) fprintf(LOG, fmt , ## args)
#define MDEBUG(fmt, args...)

// If the timestamp is further in the future than
// 100 seconds, abort
#define TIMESTAMP_ABORT_FUTURE 100

// Add a delay to increase buffers when
// frames are too far in the future
#define TIMESTAMP_MARGIN_FUTURE 0.5

typedef std::complex<float> complexf;

// Each frame contains one OFDM frame, and its
// associated timestamp
struct UHDWorkerFrameData {
    // Buffer holding frame data
    std::vector<uint8_t> buf;

    // A full timestamp contains a TIST according to standard
    // and time information within MNSC with tx_second.
    struct frame_timestamp ts;
};

enum refclk_lock_loss_behaviour_t { CRASH, IGNORE };

/* This structure is used as initial configuration for OutputUHD.
 * It must also contain all remote-controllable settings, otherwise
 * they will get lost on a modulator restart. */
struct OutputUHDConfig {
    std::string device;
    std::string usrpType; // e.g. b100, b200, usrp2

    // The USRP1 can accept two daughterboards
    std::string subDevice; // e.g. A:0

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

    /* allowed values : auto, int, sma, mimo */
    std::string refclk_src;

    /* allowed values : int, sma, mimo */
    std::string pps_src;

    /* allowed values : pos, neg */
    std::string pps_polarity;

    /* What to do when the reference clock PLL loses lock */
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;

    // muting can only be changed using the remote control
    bool muting = false;

    // static delay in microseconds
    int staticDelayUs = 0;

    // TCP port on which to serve TX and RX samples for the
    // digital pre distortion learning tool
    uint16_t dpdFeedbackServerPort = 0;
};

class OutputUHD: public ModOutput, public RemoteControllable {
    public:
        OutputUHD(OutputUHDConfig& config);
        OutputUHD(const OutputUHD& other) = delete;
        OutputUHD operator=(const OutputUHD& other) = delete;
        ~OutputUHD();

        int process(Buffer* dataIn);

        const char* name() { return "OutputUHD"; }

        void setETISource(EtiSource *etiSource);

        /*********** REMOTE CONTROL ***************/

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const;

    protected:
        EtiSource *myEtiSource = nullptr;
        OutputUHDConfig& myConf;
        uhd::usrp::multi_usrp::sptr myUsrp;
        std::shared_ptr<boost::barrier> mySyncBarrier;
        bool first_run = true;
        bool gps_fix_verified = false;
        OutputUHDFeedback uhdFeedback;

    private:
        // Resize the internal delay buffer according to the dabMode and
        // the sample rate.
        void SetDelayBuffer(unsigned int dabMode);

        // data
        // The remote-controllable static delay is in the OutputUHDConfig
        int myTFDurationMs; // TF duration in milliseconds
        std::vector<complexf> myDelayBuf;
        size_t lastLen = 0;

        // GPS Fix check variables
        int num_checks_without_gps_fix = 1;
        struct timespec first_gps_fix_check;
        struct timespec last_gps_fix_check;
        struct timespec time_last_frame;
        boost::packaged_task<bool> gps_fix_pt;
        boost::unique_future<bool> gps_fix_future;
        boost::thread gps_fix_task;

        // Wait time in seconds to get fix
        static const int initial_gps_fix_wait = 180;

        // Interval for checking the GPS at runtime
        static constexpr double gps_fix_check_interval = 10.0; // seconds

        // Asynchronous message statistics
        size_t num_underflows = 0;
        size_t num_late_packets = 0;
        size_t num_underflows_previous = 0;
        size_t num_late_packets_previous = 0;

        size_t num_frames_modulated = 0;

        uhd::tx_metadata_t md;
        bool     last_tx_time_initialised = false;
        uint32_t last_tx_second = 0;
        uint32_t last_tx_pps = 0;

        // Used to print statistics once a second
        std::chrono::steady_clock::time_point last_print_time;

        bool sourceContainsTimestamp = false;

        ThreadsafeQueue<UHDWorkerFrameData> frames;

        // Returns true if we want to verify loss of refclk
        bool refclk_loss_needs_check(void) const;
        bool suppress_refclk_loss_check = false;

        // Returns true if we want to check for the gps_timelock sensor
        bool gpsfix_needs_check(void) const;

        // Return true if the gpsdo is from ettus, false if it is the ODR
        // LEA-M8F board is used
        bool gpsdo_is_ettus(void) const;

        std::atomic<bool> running;
        boost::thread uhd_thread;
        boost::thread async_rx_thread;
        void stop_threads(void);

        uhd::tx_streamer::sptr myTxStream;

        // The worker thread decouples the modulator from UHD
        void workerthread();
        void handle_frame(const struct UHDWorkerFrameData *frame);
        void tx_frame(const struct UHDWorkerFrameData *frame, bool ts_update);

        // Poll asynchronous metadata from UHD
        void print_async_thread(void);

        void check_gps();

        void set_usrp_time();

        void initial_gps_check();
};

#endif // HAVE_OUTPUT_UHD

