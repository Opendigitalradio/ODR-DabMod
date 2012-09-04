/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Includes modifications for which no copyright is claimed
   2012, Matthias P. Braendli, matthias.braendli@mpb.li

DESCRIPTION:
   It is an output driver for the USRP family of devices, and uses
   the UHD library. This version is multi-threaded. One thread runs
   the whole modulator, and the second threads sends the data to the
   device. The two threads are synchronised using a barrier. Communication
   between the two threads is implemented using double buffering. At the
   barrier, they exchange the buffers.
*/

/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OUTPUT_UHD_H
#define OUTPUT_UHD_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
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

using namespace boost;

struct UHDWorkerFrameData {
    // Buffer holding frame data
    void* buf;

    // Full timestamp
    struct frame_timestamp ts;

    // Frame counter
    uint32_t fct;
};

enum refclk_lock_loss_behaviour_t { CRASH, IGNORE };

struct UHDWorkerData {
    uhd::usrp::multi_usrp::sptr myUsrp;
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

    // muting set by remote control
    bool muting;

    // A barrier to synchronise the two threads
    shared_ptr<barrier> sync_barrier;

    // What to do when the reference clock PLL loses lock
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;

    // The common logger
    Logger* logger;
}; 


class UHDWorker {
    public:
        UHDWorker () {
            running = false;
        }

        void start(struct UHDWorkerData *uhdworkerdata) {
            running = true;
            uhd_thread = boost::thread(&UHDWorker::process, this, uhdworkerdata);
        }

        void stop() {
            running = false;
            uhd_thread.interrupt();
            uhd_thread.join();
        }

        void process(struct UHDWorkerData *uhdworkerdata);


    private:
        struct UHDWorkerData *workerdata;
        bool running;
        boost::thread uhd_thread;

        uhd::tx_streamer::sptr myTxStream;
};

/* This structure is used as initial configuration for OutputUHD */
struct OutputUHDConfig {
    const char* device;
    unsigned sampleRate;
    double frequency;
    int txgain;
    bool enableSync;
    bool muteNoTimestamps;

    /* allowed values : auto, int, sma, mimo */
    std::string refclk_src;

    /* allowed values : int, sma, mimo */
    std::string pps_src;

    /* allowed values : pos, neg */
    std::string pps_polarity;

    /* What to do when the reference clock PLL loses lock */
    refclk_lock_loss_behaviour_t refclk_lock_loss_behaviour;
};


class OutputUHD: public ModOutput, public RemoteControllable {
    public:
        OutputUHD(
                OutputUHDConfig& config,
                Logger& logger);
        ~OutputUHD();

        int process(Buffer* dataIn, Buffer* dataOut);

        const char* name() { return "OutputUHD"; }

        void setETIReader(EtiReader *etiReader) {
            myEtiReader = etiReader;
        }

        /*********** REMOTE CONTROL ***************/
        /* Tell the controllable to enrol at the given controller /
        virtual void enrol_at(BaseRemoteController& controller) {
            controller.enrol(this);
        } // */

        /* Base function to set parameters. */
        virtual void set_parameter(string parameter, string value);

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter);


    protected:
        Logger& myLogger;
        EtiReader *myEtiReader;
        std::string myDevice;
        unsigned mySampleRate;
        int myTxGain;
        double myFrequency;
        uhd::usrp::multi_usrp::sptr myUsrp;
        shared_ptr<barrier> my_sync_barrier;
        UHDWorker worker;
        bool first_run;
        struct UHDWorkerData uwd;
        int activebuffer;
        bool mute_no_timestamps;
        bool enable_sync;

        // muting can only be changed using the remote control
        bool myMuting;

        size_t lastLen;
};


#endif // OUTPUT_UHD_H
