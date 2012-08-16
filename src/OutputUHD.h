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

    // A barrier to synchronise the two threads
    shared_ptr<barrier> sync_barrier;

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


class OutputUHD: public ModOutput, public RemoteControl {
    public:
        OutputUHD(const char* device,
                unsigned sampleRate,
                double frequency,
                int txgain,
                bool enableSync,
                bool muteNoTimestamps,
                Logger& logger);
        ~OutputUHD();

        int process(Buffer* dataIn, Buffer* dataOut);

        const char* name() { return "OutputUHD"; }

        void setETIReader(EtiReader *etiReader) {
            myEtiReader = etiReader;
        }

        /*********** REMOTE CONTROL ***************/
        virtual std::string get_rc_name() { return "uhd"; }
        
        /* Tell the controllable to enrol at the given controller /
        virtual void enrol_at(BaseRemoteController& controller) {
            controller.enrol(this);
        } // */

        /* Return a list of possible parameters that can be set */
        list<string> get_supported_parameters() {
            list<string> parameterlist;
            for (list< vector<string> >::iterator it = parameters_.begin(); it != parameters_.end(); it++) {
                parameterlist.push_back((*it)[0]);
            }
            return parameterlist;
        }

        /* Return a mapping of the descriptions of all parameters */
        virtual std::list< std::vector<std::string> > get_parameter_descriptions() = 0;

        /* Base function to set parameters. */
        virtual void set_parameter(string parameter, string value) = 0;

        /* Convenience functions for other common types */
        virtual void set_parameter(string parameter, double value) = 0;
        virtual void set_parameter(string parameter, long value) = 0;

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter) = 0;


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

        size_t lastLen;


        /*********** REMOTE CONTROL ***************/
};


#endif // OUTPUT_UHD_H
