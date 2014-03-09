/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Includes modifications for which no copyright is claimed
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
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

#define ENABLE_UHD 1

#include "OutputUHD.h"
#include "PcDebug.h"
#include "Log.h"
#include "RemoteControl.h"

#include <iostream>
#include <assert.h>
#include <stdexcept>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

typedef std::complex<float> complexf;

OutputUHD::OutputUHD(
        OutputUHDConfig& config,
        Logger& logger) :
    ModOutput(ModFormat(1), ModFormat(0)),
    RemoteControllable("uhd"),
    myLogger(logger)
{

    mySampleRate = config.sampleRate;
    myTxGain = config.txgain;
    myFrequency = config.frequency;
    mute_no_timestamps = config.muteNoTimestamps;
    enable_sync = config.enableSync;
    myDevice = config.device;
    myMuting = 0;

    MDEBUG("OutputUHD::OutputUHD(device: %s) @ %p\n",
            myDevice.c_str(), this);

    /* register the parameters that can be remote controlled */
    RC_ADD_PARAMETER(txgain, "UHD analog daughterboard TX gain");
    RC_ADD_PARAMETER(freq,   "UHD transmission frequency");
    RC_ADD_PARAMETER(muting, "mute the output by stopping the transmitter");

    
#if ENABLE_UHD
    uhd::set_thread_priority_safe();

    //create a usrp device
    MDEBUG("OutputUHD:Creating the usrp device with: %s...\n",
            myDevice.c_str());

    myUsrp = uhd::usrp::multi_usrp::make(myDevice);

    MDEBUG("OutputUHD:Using device: %s...\n", myUsrp->get_pp_string().c_str());

    MDEBUG("OutputUHD:Setting REFCLK and PPS input...\n");

    myUsrp->set_clock_source(config.refclk_src);
    myUsrp->set_time_source(config.pps_src);

    std::cerr << "UHD clock source is " <<
        myUsrp->get_clock_source(0) << std::endl;

    std::cerr << "UHD time source is " <<
        myUsrp->get_time_source(0) << std::endl;

    //set the tx sample rate
    MDEBUG("OutputUHD:Setting rate to %d...\n", mySampleRate);
    myUsrp->set_tx_rate(mySampleRate);
    MDEBUG("OutputUHD:Actual TX Rate: %f Msps...\n", myUsrp->get_tx_rate());

    if (mySampleRate != myUsrp->get_tx_rate()) {
        MDEBUG("OutputUHD: Cannot set sample\n");
        throw std::runtime_error("Cannot set USRP sample rate. Aborted.");
    }

    //set the centre frequency
    MDEBUG("OutputUHD:Setting freq to %f...\n", myFrequency);
    myUsrp->set_tx_freq(myFrequency);
    myFrequency = myUsrp->get_tx_freq();
    MDEBUG("OutputUHD:Actual frequency: %f\n", myFrequency);

    myUsrp->set_tx_gain(myTxGain);
    MDEBUG("OutputUHD:Actual TX Gain: %f ...\n", myUsrp->get_tx_gain());

    MDEBUG("OutputUHD:Mute on missing timestamps: %s ...\n", mute_no_timestamps ? "enabled" : "disabled");

    if (enable_sync && (config.pps_src == "none")) {
        myLogger.level(warn) << "OutputUHD: WARNING: you are using synchronous transmission without PPS input!";
        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now)) {
            perror("OutputUHD:Error: could not get time: ");
            myLogger.level(error) << "OutputUHD: could not get time";
        }
        else {
            myUsrp->set_time_now(uhd::time_spec_t(now.tv_sec));
            myLogger.level(info) << "OutputUHD: Setting USRP time to " <<
                    uhd::time_spec_t(now.tv_sec).get_real_secs();
        }
    }

    if (config.pps_src != "none") {
        /* handling time for synchronisation: wait until the next full
         * second, and set the USRP time at next PPS */
        struct timespec now;
        time_t seconds;
        if (clock_gettime(CLOCK_REALTIME, &now)) {
            myLogger.level(error) << "OutputUHD: could not get time :" <<
                strerror(errno);
            throw std::runtime_error("OutputUHD: could not get time.");
        }
        else {
            seconds = now.tv_sec;

            MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
            while (seconds + 1 > now.tv_sec) {
                usleep(1);
                if (clock_gettime(CLOCK_REALTIME, &now)) {
                    myLogger.level(error) << "OutputUHD: could not get time :" <<
                        strerror(errno);
                    throw std::runtime_error("OutputUHD: could not get time.");
                }
            }
            MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
            /* We are now shortly after the second change. */

            usleep(200000); // 200ms, we want the PPS to be later
            myUsrp->set_time_unknown_pps(uhd::time_spec_t(seconds + 2));
            myLogger.level(info) << "OutputUHD: Setting USRP time next pps to " <<
                    uhd::time_spec_t(seconds + 2).get_real_secs();
        }

        usleep(1e6);
        myLogger.log(info,  "OutputUHD: USRP time %f\n",
                myUsrp->get_time_now().get_real_secs());
    }


    // preparing output thread worker data
    uwd.myUsrp = myUsrp;
#else
    myLogger.level(error) << "OutputUHD: UHD initialisation disabled at compile-time";
#endif

    uwd.frame0.ts.timestamp_valid = false;
    uwd.frame1.ts.timestamp_valid = false;
    uwd.sampleRate = mySampleRate;
    uwd.sourceContainsTimestamp = false;
    uwd.muteNoTimestamps = mute_no_timestamps;
    uwd.logger = &myLogger;
    uwd.refclk_lock_loss_behaviour = config.refclk_lock_loss_behaviour;

    // Since we don't know the buffer size, we cannot initialise
    // the buffers here
    first_run = true;

    shared_ptr<barrier> b(new barrier(2));
    mySyncBarrier = b;
    uwd.sync_barrier = b;

    worker.start(&uwd);

    MDEBUG("OutputUHD:UHD ready.\n");
}


OutputUHD::~OutputUHD()
{
    MDEBUG("OutputUHD::~OutputUHD() @ %p\n", this);
    worker.stop();
}

int OutputUHD::process(Buffer* dataIn, Buffer* dataOut)
{
    struct frame_timestamp ts;

    uwd.muting = myMuting;


    // On the first call, we must do some allocation and we must fill
    // the first buffer
    // We will only wait on the barrier on the subsequent calls to 
    // OutputUHD::process
    if (first_run) {
        myLogger.level(debug) << "OutputUHD: UHD initialising...";

        uwd.bufsize = dataIn->getLength();
        uwd.frame0.buf = malloc(uwd.bufsize);
        uwd.frame1.buf = malloc(uwd.bufsize);

        uwd.sourceContainsTimestamp = enable_sync && myEtiReader->sourceContainsTimestamp();

        // The worker begins by transmitting buf0
        memcpy(uwd.frame0.buf, dataIn->getData(), uwd.bufsize);

        myEtiReader->calculateTimestamp(ts);
        uwd.frame0.ts = ts;
        uwd.frame0.fct = myEtiReader->getFCT();

        activebuffer = 1;

        lastLen = uwd.bufsize;
        first_run = false;
        myLogger.level(debug) << "OutputUHD: UHD initialising complete";
    }
    else {

        if (lastLen != dataIn->getLength()) {
            // I expect that this never happens.
            myLogger.level(emerg) << "OutputUHD: Fatal error, input length changed from " <<
                    lastLen << " to " << dataIn->getLength();
            throw std::runtime_error("Non-constant input length!");
        }
        mySyncBarrier.get()->wait();

        // write into the our buffer while
        // the worker sends the other.

        myEtiReader->calculateTimestamp(ts);
        uwd.sourceContainsTimestamp = enable_sync && myEtiReader->sourceContainsTimestamp();

        if (activebuffer == 0) {
            memcpy(uwd.frame0.buf, dataIn->getData(), uwd.bufsize);

            uwd.frame0.ts = ts;
            uwd.frame0.fct = myEtiReader->getFCT();
        }
        else if (activebuffer == 1) {
            memcpy(uwd.frame1.buf, dataIn->getData(), uwd.bufsize);

            uwd.frame1.ts = ts;
            uwd.frame1.fct = myEtiReader->getFCT();
        }

        activebuffer = (activebuffer + 1) % 2;
    }

    return uwd.bufsize;

}

void UHDWorker::process()
{
    int workerbuffer  = 0;
    time_t tx_second = 0;
    double pps_offset = 0;
    double last_pps   = 2.0;
    double usrp_time;

    //const struct timespec hundred_nano = {0, 100};

    size_t sizeIn;
    struct UHDWorkerFrameData* frame;

    size_t num_acc_samps; //number of accumulated samples
    //int write_fail_count;

#if ENABLE_UHD
    // Transmit timeout
    const double timeout = 0.2;

    uhd::stream_args_t stream_args("fc32"); //complex floats
    uhd::tx_streamer::sptr myTxStream = uwd->myUsrp->get_tx_stream(stream_args);
    size_t bufsize = myTxStream->get_max_num_samps();
#endif

    bool check_refclk_loss = true;
    const complexf* in;

    uhd::tx_metadata_t md;
    md.start_of_burst = false;
    md.end_of_burst = false;

    while (running) {
        md.has_time_spec = false;
        md.time_spec = uhd::time_spec_t(0.0);
        num_acc_samps = 0;
        //write_fail_count = 0;

        /* Wait for barrier */
        // this wait will hopefully always be the second one
        // because modulation should be quicker than transmission
        uwd->sync_barrier.get()->wait();

        if (workerbuffer == 0) {
            frame = &(uwd->frame0);
        }
        else if (workerbuffer == 1) {
            frame = &(uwd->frame1);
        }
        else {
            throw std::runtime_error("UHDWorker.process: workerbuffer is neither 0 nor 1 !");
        }

        in = reinterpret_cast<const complexf*>(frame->buf);
        pps_offset = frame->ts.timestamp_pps_offset;

        // Tx second from MNSC
        tx_second = frame->ts.timestamp_sec;

        sizeIn = uwd->bufsize / sizeof(complexf);

#if ENABLE_UHD
        // Check for ref_lock
        if (check_refclk_loss)
        {
            try {
                // TODO: Is this check specific to the B100 and USRP2 ?
                if (! uwd->myUsrp->get_mboard_sensor("ref_locked", 0).to_bool()) {
                    uwd->logger->log(alert, "OutputUHD: External reference clock lock lost !");
                    if (uwd->refclk_lock_loss_behaviour == CRASH) {
                        throw std::runtime_error("OutputUHD: External reference clock lock lost.");
                    }
                }
            }
            catch (uhd::lookup_error &e) {
                check_refclk_loss = false;
                uwd->logger->log(warn,
                        "OutputUHD: This USRP does not have mboard sensor for ext clock loss. Check disabled.");
            }
        }

        usrp_time = uwd->myUsrp->get_time_now().get_real_secs();
#else
        usrp_time = 0;
#endif

        if (uwd->sourceContainsTimestamp) {
            if (!frame->ts.timestamp_valid) {
                /* We have not received a full timestamp through
                 * MNSC. We sleep through the frame.
                 */
                uwd->logger->level(info) << "OutputUHD: Throwing sample " <<
                    frame->fct << " away: incomplete timestamp " << tx_second << " + " << pps_offset;
                usleep(20000); //TODO should this be TM-dependant ?
                goto loopend;
            }

            md.has_time_spec = true;
            md.time_spec = uhd::time_spec_t(tx_second, pps_offset);

            // md is defined, let's do some checks
            if (md.time_spec.get_real_secs() + 0.2 < usrp_time) {
                uwd->logger->level(warn) <<
                    "OutputUHD: Timestamp in the past! offset: " <<
                    md.time_spec.get_real_secs() - usrp_time <<
                    "  (" << usrp_time << ")"
                    " frame " << frame->fct <<
                    ", tx_second " << tx_second <<
                    ", pps " << pps_offset;
                goto loopend; //skip the frame
            }

#if ENABLE_UHD
            if (md.time_spec.get_real_secs() > usrp_time + TIMESTAMP_MARGIN_FUTURE) {
                uwd->logger->level(warn) <<
                        "OutputUHD: Timestamp too far in the future! offset: " <<
                        md.time_spec.get_real_secs() - usrp_time;
                usleep(20000); //sleep so as to fill buffers
            }

            if (md.time_spec.get_real_secs() > usrp_time + TIMESTAMP_ABORT_FUTURE) {
                uwd->logger->level(error) <<
                        "OutputUHD: Timestamp way too far in the future! offset: " <<
                        md.time_spec.get_real_secs() - usrp_time;
                throw std::runtime_error("Timestamp error. Aborted.");
            }
#endif

            if (last_pps > pps_offset) {
                uwd->logger->log(info,
                        "OutputUHD (usrp time: %f): frame %d;  tx_second %zu; pps %.9f\n",
                        usrp_time,
                        frame->fct, tx_second, pps_offset);
            }

        }
        else { // !uwd->sourceContainsTimestamp
            if (uwd->muting || uwd->muteNoTimestamps) {
                /* There was some error decoding the timestamp
                */
                if (uwd->muting) {
                    uwd->logger->log(info, "OutputUHD: Muting sample %d requested\n",
                            frame->fct);
                }
                else {
                    uwd->logger->log(info, "OutputUHD: Muting sample %d : no timestamp\n",
                            frame->fct);
                }
                usleep(20000);
                goto loopend;
            }
        }

#if ENABLE_UHD
        PDEBUG("UHDWorker::process:max_num_samps: %zu.\n",
                myTxStream->get_max_num_samps());

        /*
           size_t num_tx_samps = myTxStream->send(
           dataIn, sizeIn, md, timeout);

           MDEBUG("UHDWorker::process:num_tx_samps: %zu.\n", num_tx_samps);
           */
        while (running && !uwd->muting && (num_acc_samps < sizeIn)) {
            size_t samps_to_send = std::min(sizeIn - num_acc_samps, bufsize);

            //ensure the the last packet has EOB set if the timestamps has been refreshed
            //and need to be reconsidered.
            md.end_of_burst = (frame->ts.timestamp_refresh && (samps_to_send <= bufsize));

            //send a single packet
            size_t num_tx_samps = myTxStream->send(
                    &in[num_acc_samps],
                    samps_to_send, md, timeout);

            num_acc_samps += num_tx_samps;

            md.time_spec = uhd::time_spec_t(tx_second, pps_offset)
                + uhd::time_spec_t(0, num_acc_samps/uwd->sampleRate);

            /*
               fprintf(stderr, "*** pps_offset %f, md.time_spec %f, usrp->now %f\n",
               pps_offset,
               md.time_spec.get_real_secs(),
               uwd->myUsrp->get_time_now().get_real_secs());
            // */


            if (num_tx_samps == 0) {
#if 1
                uwd->logger->log(warn,
                        "UHDWorker::process() unable to write to device, skipping frame!\n");
                break;
#else
                // This has been disabled, because if there is a write failure,
                // we'd better not insist and try to go on transmitting future
                // frames.
                // The goal is not to try to send by all means possible. It's
                // more important to make sure the SFN is not disturbed.

                fprintf(stderr, "F");
                nanosleep(&hundred_nano, NULL);
                write_fail_count++;
                if (write_fail_count >= 3) {
                    double ts = md.time_spec.get_real_secs();
                    double t_usrp = uwd->myUsrp->get_time_now().get_real_secs();

                    fprintf(stderr, "*** USRP write fail count %d\n", write_fail_count);
                    fprintf(stderr, "*** delta %f, md.time_spec %f, usrp->now %f\n",
                            ts - t_usrp,
                            ts, t_usrp);

                    fprintf(stderr, "UHDWorker::process() unable to write to device, skipping frame!\n");
                    break;
                }
#endif
            }

            uhd::async_metadata_t async_md;
            if (uwd->myUsrp->get_device()->recv_async_msg(async_md, 0)) {
                const char* uhd_async_message = "";
                bool failure = true;
                switch (async_md.event_code) {
                    case uhd::async_metadata_t::EVENT_CODE_BURST_ACK:
                        failure = false;
                        break;
                    case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW:
                        uhd_async_message = "Underflow";
                        break;
                    case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR:
                        uhd_async_message = "Packet loss between host and device.";
                        break;
                    case uhd::async_metadata_t::EVENT_CODE_TIME_ERROR:
                        uhd_async_message = "Packet had time that was late.";
                        break;
                    case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET:
                        uhd_async_message = "Underflow occurred inside a packet.";
                        break;
                    case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR_IN_BURST:
                        uhd_async_message = "Packet loss within a burst.";
                        break;
                    default:
                        uhd_async_message = "unknown event code";
                        break;
                }

                if (failure) {
                    uwd->logger->level(alert) << "Near frame " <<
                            frame->fct << ": Received Async UHD Message '" << 
                            uhd_async_message << "'";

                }
            }

            /*
               bool got_async_burst_ack = false;
            //loop through all messages for the ACK packet (may have underflow messages in queue)
            while (not got_async_burst_ack and uwd->myUsrp->get_device()->recv_async_msg(async_md, 0.2)){
            got_async_burst_ack = (async_md.event_code == uhd::async_metadata_t::EVENT_CODE_BURST_ACK);
            }
            //std::cerr << (got_async_burst_ack? "success" : "fail") << std::endl;
            // */


        }
#else // ENABLE_UHD
        uwd->logger->log(info, "OutputUHD UHD DISABLED: Sample %d : valid timestamp %zu + %f\n",
                frame->fct,
                tx_second,
                pps_offset);
        usleep(23000); // 23ms, a bit faster than realtime for TM 2
#endif

        last_pps = pps_offset;

loopend:
        // swap buffers
        workerbuffer = (workerbuffer + 1) % 2;
    }
}


void OutputUHD::set_parameter(string parameter, string value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "txgain") {
        ss >> myTxGain;
#if ENABLE_UHD
        myUsrp->set_tx_gain(myTxGain);
#endif
    }
    else if (parameter == "freq") {
        ss >> myFrequency;
#if ENABLE_UHD
        myUsrp->set_tx_freq(myFrequency);
        myFrequency = myUsrp->get_tx_freq();
#endif
    }
    else if (parameter == "muting") {
        ss >> myMuting;
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

string OutputUHD::get_parameter(string parameter)
{
    stringstream ss;
    if (parameter == "txgain") {
        ss << myTxGain;
    }
    else if (parameter == "freq") {
        ss << myFrequency;
    }
    else if (parameter == "muting") {
        ss << myMuting;
    }
    else {
        ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

