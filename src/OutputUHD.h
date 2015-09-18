/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014, 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver for the USRP family of devices, and uses the UHD
   library. This version is multi-threaded. A separate thread sends the data to
   the device.

   Data between the modulator and the UHD thread is exchanged by swapping
   buffers at a synchronisation barrier.
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

#ifndef OUTPUT_UHD_H
#define OUTPUT_UHD_H

#define FAKE_UHD 0

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#ifdef HAVE_OUTPUT_UHD

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <list>
#include <memory>
#include <string>

#include "Log.h"
#include "ModOutput.h"
#include "EtiReader.h"
#include "TimestampDecoder.h"
#include "RemoteControl.h"

#include <stdio.h>
#include <sys/types.h>

#define MDEBUG(fmt, args...) fprintf (LOG, fmt , ## args) 
//#define MDEBUG(fmt, args...)

// If the timestamp is further in the future than
// 100 seconds, abort
#define TIMESTAMP_ABORT_FUTURE 100

// Add a delay to increase buffers when
// frames are too far in the future
#define TIMESTAMP_MARGIN_FUTURE 0.5

typedef std::complex<float> complexf;

struct UHDWorkerFrameData {
    // Buffer holding frame data
    void* buf;

    // Full timestamp
    struct frame_timestamp ts;
};

struct fct_discontinuity_error : public std::exception
{
  const char* what () const throw ()
  {
    return "FCT discontinuity detected";
  }
};

enum refclk_lock_loss_behaviour_t { CRASH, IGNORE };

struct UHDWorkerData {
    bool running;
    bool failed_due_to_fct;

#if FAKE_UHD == 0
    uhd::usrp::multi_usrp::sptr myUsrp;
#endif
    unsigned sampleRate;

    // Double buffering between the two threads
    // Each buffer contains one OFDM frame, and it's
    // associated timestamp
    // A full timestamp contains a TIST according to standard
    // and time information within MNSC with tx_second.
    bool sourceContainsTimestamp;

    // When working with timestamps, mute the frames that
    // do not have a timestamp
    bool muteNoTimestamps;

    struct UHDWorkerFrameData frame0;
    struct UHDWorkerFrameData frame1;
    size_t bufsize; // in bytes

    // If we want to verify loss of refclk
    bool check_refclk_loss;

    // If we want to check for the gps_timelock sensor
    bool check_gpsfix;

    bool gpsdo_is_ettus; // Set to false in case the ODR LEA-M8F board is used

    // muting set by remote control
    bool muting;

    // A barrier to synchronise the two threads
    std::shared_ptr<boost::barrier> sync_barrier;

    // What to do when the reference clock PLL loses lock
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;

    // What transmission mode we're using defines by how
    // much the FCT should increment for each
    // transmission frame.
    int fct_increment;
};


class UHDWorker {
    public:
        void start(struct UHDWorkerData *uhdworkerdata) {
            uwd = uhdworkerdata;

            uwd->running = true;
            uwd->failed_due_to_fct = false;
            uhd_thread = boost::thread(&UHDWorker::process_errhandler, this);
        }

        void stop() {
            if (uwd) {
                uwd->running = false;
            }
            uhd_thread.interrupt();
            uhd_thread.join();
        }

    private:
        // Asynchronous message statistics
        int num_underflows;
        int num_late_packets;

        bool fct_discontinuity;
        int expected_next_fct;
        uhd::tx_metadata_t md;
        time_t tx_second;
        double pps_offset;
        double last_pps;

        void print_async_metadata(const struct UHDWorkerFrameData *frame);

        void handle_frame(const struct UHDWorkerFrameData *frame);
        void tx_frame(const struct UHDWorkerFrameData *frame);

        struct UHDWorkerData *uwd;
        boost::thread uhd_thread;

        uhd::tx_streamer::sptr myTxStream;

        void process();
        void process_errhandler();
};

/* This structure is used as initial configuration for OutputUHD.
 * It must also contain all remote-controllable settings, otherwise
 * they will get lost on a modulator restart. */
struct OutputUHDConfig {
    std::string device;
    std::string usrpType; // e.g. b100, b200, usrp2

    // The USRP1 can accept two daughterboards
    std::string subDevice; // e.g. A:0

    long masterClockRate;
    unsigned sampleRate;
    double frequency;
    double txgain;
    bool enableSync;
    bool muteNoTimestamps;
    unsigned dabMode;
    unsigned maxGPSHoldoverTime;

    /* allowed values : auto, int, sma, mimo */
    std::string refclk_src;

    /* allowed values : int, sma, mimo */
    std::string pps_src;

    /* allowed values : pos, neg */
    std::string pps_polarity;

    /* What to do when the reference clock PLL loses lock */
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;

    // muting can only be changed using the remote control
    bool muting;

    // static delay in microseconds
    int staticDelayUs;
};


class OutputUHD: public ModOutput, public RemoteControllable {
    public:

        OutputUHD(OutputUHDConfig& config);
        ~OutputUHD();

        int process(Buffer* dataIn, Buffer* dataOut);

        const char* name() { return "OutputUHD"; }

        void setETIReader(EtiReader *etiReader) {
            myEtiReader = etiReader;
        }

        /*********** REMOTE CONTROL ***************/
        /* virtual void enrol_at(BaseRemoteController& controller)
         * is inherited
         */

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const;


    protected:
        OutputUHD(const OutputUHD& other);
        OutputUHD& operator=(const OutputUHD& other);

        EtiReader *myEtiReader;
        OutputUHDConfig& myConf;
        uhd::usrp::multi_usrp::sptr myUsrp;
        std::shared_ptr<boost::barrier> mySyncBarrier;
        UHDWorker worker;
        bool first_run;
        bool gps_fix_verified;
        struct UHDWorkerData uwd;
        int activebuffer;

    private:
        // Resize the internal delay buffer according to the dabMode and
        // the sample rate.
        void SetDelayBuffer(unsigned int dabMode);

        // data
        // The remote-controllable static delay is in the OutputUHDConfig
        int myTFDurationMs; // TF duration in milliseconds
        std::vector<complexf> myDelayBuf;
        size_t lastLen;

        // GPS Fix check variables
        int num_checks_without_gps_fix;
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

        void check_gps();

        void set_usrp_time();

        void initial_gps_check();
};

#endif // HAVE_OUTPUT_UHD

#endif // OUTPUT_UHD_H

