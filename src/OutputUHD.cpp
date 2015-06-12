/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014, 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#include "OutputUHD.h"

#ifdef HAVE_OUTPUT_UHD

#include "PcDebug.h"
#include "Log.h"
#include "RemoteControl.h"

#include <boost/thread/future.hpp>

#include <uhd/utils/msg.hpp>

#include <cmath>
#include <iostream>
#include <assert.h>
#include <stdexcept>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

using namespace boost;
using namespace std;

typedef std::complex<float> complexf;

void uhd_msg_handler(uhd::msg::type_t type, const std::string &msg)
{
    if (type == uhd::msg::warning) {
        etiLog.level(warn) << "UHD Warning: " << msg;
    }
    else if (type == uhd::msg::error) {
        etiLog.level(error) << "UHD Error: " << msg;
    }
}

OutputUHD::OutputUHD(
        const OutputUHDConfig& config) :
    ModOutput(ModFormat(1), ModFormat(0)),
    RemoteControllable("uhd"),
    myConf(config),
    // Since we don't know the buffer size, we cannot initialise
    // the buffers at object initialisation.
    first_run(true),
    activebuffer(1),
    myDelayBuf(0)

{
    myMuting = 0; // is remote-controllable
    myStaticDelayUs = 0; // is remote-controllable

#if FAKE_UHD
    MDEBUG("OutputUHD:Using fake UHD output");
#else
    std::stringstream device;
    device << myConf.device;

    if (myConf.masterClockRate != 0) {
        if (device.str() != "") {
            device << ",";
        }
        device << "master_clock_rate=" << myConf.masterClockRate;
    }

    if (myConf.usrpType != "") {
        if (device.str() != "") {
            device << ",";
        }
        device << "type=" << myConf.usrpType;
    }

    MDEBUG("OutputUHD::OutputUHD(device: %s) @ %p\n",
            device.str().c_str(), this);

    /* register the parameters that can be remote controlled */
    RC_ADD_PARAMETER(txgain, "UHD analog daughterboard TX gain");
    RC_ADD_PARAMETER(freq,   "UHD transmission frequency");
    RC_ADD_PARAMETER(muting, "Mute the output by stopping the transmitter");
    RC_ADD_PARAMETER(staticdelay, "Set static delay (uS) between 0 and 96000");

    // TODO: find out how to use boost::bind to give the logger to the
    // uhd_msg_handler
    uhd::msg::register_handler(uhd_msg_handler);

    uhd::set_thread_priority_safe();

    //create a usrp device
    MDEBUG("OutputUHD:Creating the usrp device with: %s...\n",
            device.str().c_str());

    myUsrp = uhd::usrp::multi_usrp::make(device.str());

    MDEBUG("OutputUHD:Using device: %s...\n", myUsrp->get_pp_string().c_str());

    if (myConf.masterClockRate != 0.0) {
        double master_clk_rate = myUsrp->get_master_clock_rate();
        MDEBUG("OutputUHD:Checking master clock rate: %f...\n", master_clk_rate);

        if (fabs(master_clk_rate - myConf.masterClockRate) >
                (myConf.masterClockRate * 1e-6)) {
            throw std::runtime_error("Cannot set USRP master_clock_rate. Aborted.");
        }
    }

    MDEBUG("OutputUHD:Setting REFCLK and PPS input...\n");

    myUsrp->set_clock_source(myConf.refclk_src);
    myUsrp->set_time_source(myConf.pps_src);

    if (myConf.subDevice != "") {
        myUsrp->set_tx_subdev_spec(uhd::usrp::subdev_spec_t(myConf.subDevice),
                uhd::usrp::multi_usrp::ALL_MBOARDS);
    }

    std::cerr << "UHD clock source is " <<
        myUsrp->get_clock_source(0) << std::endl;

    std::cerr << "UHD time source is " <<
        myUsrp->get_time_source(0) << std::endl;

    //set the tx sample rate
    MDEBUG("OutputUHD:Setting rate to %d...\n", myConf.sampleRate);
    myUsrp->set_tx_rate(myConf.sampleRate);
    MDEBUG("OutputUHD:Actual TX Rate: %f Msps...\n", myUsrp->get_tx_rate());

    if (fabs(myUsrp->get_tx_rate() / myConf.sampleRate) >
             myConf.sampleRate * 1e-6) {
        MDEBUG("OutputUHD: Cannot set sample\n");
        throw std::runtime_error("Cannot set USRP sample rate. Aborted.");
    }

    //set the centre frequency
    MDEBUG("OutputUHD:Setting freq to %f...\n", myConf.frequency);
    myUsrp->set_tx_freq(myConf.frequency);
    myConf.frequency = myUsrp->get_tx_freq();
    MDEBUG("OutputUHD:Actual frequency: %f\n", myConf.frequency);

    myUsrp->set_tx_gain(myConf.txgain);
    MDEBUG("OutputUHD:Actual TX Gain: %f ...\n", myUsrp->get_tx_gain());

    MDEBUG("OutputUHD:Mute on missing timestamps: %s ...\n",
            myConf.muteNoTimestamps ? "enabled" : "disabled");

    if (myConf.enableSync && (myConf.pps_src == "none")) {
        etiLog.level(warn) <<
            "OutputUHD: WARNING:"
            " you are using synchronous transmission without PPS input!";

        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now)) {
            perror("OutputUHD:Error: could not get time: ");
            etiLog.level(error) << "OutputUHD: could not get time";
        }
        else {
            myUsrp->set_time_now(uhd::time_spec_t(now.tv_sec));
            etiLog.level(info) << "OutputUHD: Setting USRP time to " <<
                    uhd::time_spec_t(now.tv_sec).get_real_secs();
        }
    }

    if (myConf.pps_src != "none") {
        /* handling time for synchronisation: wait until the next full
         * second, and set the USRP time at next PPS */
        struct timespec now;
        time_t seconds;
        if (clock_gettime(CLOCK_REALTIME, &now)) {
            etiLog.level(error) << "OutputUHD: could not get time :" <<
                strerror(errno);
            throw std::runtime_error("OutputUHD: could not get time.");
        }
        else {
            seconds = now.tv_sec;

            MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
            while (seconds + 1 > now.tv_sec) {
                usleep(1);
                if (clock_gettime(CLOCK_REALTIME, &now)) {
                    etiLog.level(error) << "OutputUHD: could not get time :" <<
                        strerror(errno);
                    throw std::runtime_error("OutputUHD: could not get time.");
                }
            }
            MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
            /* We are now shortly after the second change. */

            usleep(200000); // 200ms, we want the PPS to be later
            myUsrp->set_time_unknown_pps(uhd::time_spec_t(seconds + 2));
            etiLog.level(info) << "OutputUHD: Setting USRP time next pps to " <<
                    uhd::time_spec_t(seconds + 2).get_real_secs();
        }

        usleep(1e6);
        etiLog.log(info,  "OutputUHD: USRP time %f\n",
                myUsrp->get_time_now().get_real_secs());
    }


    // preparing output thread worker data
    uwd.myUsrp = myUsrp;
#endif

    uwd.frame0.ts.timestamp_valid = false;
    uwd.frame1.ts.timestamp_valid = false;
    uwd.sampleRate = myConf.sampleRate;
    uwd.sourceContainsTimestamp = false;
    uwd.muteNoTimestamps = myConf.muteNoTimestamps;
    uwd.refclk_lock_loss_behaviour = myConf.refclk_lock_loss_behaviour;

    if (myConf.refclk_src == "internal") {
        uwd.check_refclk_loss = false;
        uwd.check_gpsfix = false;
    }
    else if (myConf.refclk_src == "gpsdo") {
        uwd.check_refclk_loss = true;
        uwd.check_gpsfix = (myConf.maxGPSHoldoverTime != 0);
    }
    else {
        uwd.check_refclk_loss = true;
        uwd.check_gpsfix = false;
    }

    uwd.max_gps_holdover = myConf.maxGPSHoldoverTime;

    SetDelayBuffer(config.dabMode);

    shared_ptr<barrier> b(new barrier(2));
    mySyncBarrier = b;
    uwd.sync_barrier = b;

    MDEBUG("OutputUHD:UHD ready.\n");
}


OutputUHD::~OutputUHD()
{
    MDEBUG("OutputUHD::~OutputUHD() @ %p\n", this);
    worker.stop();
    if (!first_run) {
        free(uwd.frame0.buf);
        free(uwd.frame1.buf);
    }
}

void OutputUHD::SetDelayBuffer(unsigned int dabMode)
{
    // find out the duration of the transmission frame (Table 2 in ETSI 300 401)
    switch (dabMode) {
        case 0: // could happen when called from constructor and we take the mode from ETI
            myTFDurationMs = 0;
            break;
        case 1:
            myTFDurationMs = 96;
            break;
        case 2:
            myTFDurationMs = 24;
            break;
        case 3:
            myTFDurationMs = 24;
            break;
        case 4:
            myTFDurationMs = 48;
            break;
        default:
            throw std::runtime_error("OutputUHD: invalid DAB mode");
    }
    // The buffer size equals the number of samples per transmission frame so
    // we calculate it by multiplying the duration of the transmission frame
    // with the samplerate.
    myDelayBuf.resize(myTFDurationMs * myConf.sampleRate / 1000);
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
        etiLog.level(debug) << "OutputUHD: UHD initialising...";

        worker.start(&uwd);

        uwd.bufsize = dataIn->getLength();
        uwd.frame0.buf = malloc(uwd.bufsize);
        uwd.frame1.buf = malloc(uwd.bufsize);

        uwd.sourceContainsTimestamp = myConf.enableSync &&
            myEtiReader->sourceContainsTimestamp();

        // The worker begins by transmitting buf0
        memcpy(uwd.frame0.buf, dataIn->getData(), uwd.bufsize);

        myEtiReader->calculateTimestamp(ts);
        uwd.frame0.ts = ts;

        switch (myEtiReader->getMode()) {
            case 1: uwd.fct_increment = 4; break;
            case 2:
            case 3: uwd.fct_increment = 1; break;
            case 4: uwd.fct_increment = 2; break;
            default: break;
        }

        // we only set the delay buffer from the dab mode signaled in ETI if the
        // dab mode was not set in contructor
        if (myTFDurationMs == 0) {
            SetDelayBuffer(myEtiReader->getMode());
        }

        activebuffer = 1;

        lastLen = uwd.bufsize;
        first_run = false;
        etiLog.level(debug) << "OutputUHD: UHD initialising complete";
    }
    else {

        if (lastLen != dataIn->getLength()) {
            // I expect that this never happens.
            etiLog.level(emerg) <<
                "OutputUHD: Fatal error, input length changed from " << lastLen <<
                " to " << dataIn->getLength();
            throw std::runtime_error("Non-constant input length!");
        }

        mySyncBarrier.get()->wait();

        if (!uwd.running) {
            worker.stop();
            first_run = true;
            if (uwd.failed_due_to_fct) {
                throw fct_discontinuity_error();
            }
            else {
                etiLog.level(error) <<
                    "OutputUHD: Error, UHD worker failed";
                throw std::runtime_error("UHD worker failed");
            }
        }

        // write into the our buffer while
        // the worker sends the other.

        myEtiReader->calculateTimestamp(ts);
        uwd.sourceContainsTimestamp = myConf.enableSync &&
            myEtiReader->sourceContainsTimestamp();

        // calculate delay
        uint32_t noSampleDelay = (myStaticDelayUs * (myConf.sampleRate / 1000)) / 1000;
        uint32_t noByteDelay = noSampleDelay * sizeof(complexf);

        uint8_t* pInData = (uint8_t*) dataIn->getData();
        if (activebuffer == 0) {
            uint8_t *pTmp = (uint8_t*) uwd.frame0.buf;
            // copy remain from delaybuf
            memcpy(pTmp, &myDelayBuf[0], noByteDelay);
            // copy new data
            memcpy(&pTmp[noByteDelay], pInData, uwd.bufsize - noByteDelay);
            // copy remaining data to delay buf
            memcpy(&myDelayBuf[0], &pInData[uwd.bufsize - noByteDelay], noByteDelay);

            uwd.frame0.ts = ts;
        }
        else if (activebuffer == 1) {
            uint8_t *pTmp = (uint8_t*) uwd.frame1.buf;
            // copy remain from delaybuf
            memcpy(pTmp, &myDelayBuf[0], noByteDelay);
            // copy new data
            memcpy(&pTmp[noByteDelay], pInData, uwd.bufsize - noByteDelay);
            // copy remaining data to delay buf
            memcpy(&myDelayBuf[0], &pInData[uwd.bufsize - noByteDelay], noByteDelay);

            uwd.frame1.ts = ts;
        }

        activebuffer = (activebuffer + 1) % 2;
    }

    return uwd.bufsize;

}

void UHDWorker::process_errhandler()
{
    try {
        process();
    }
    catch (fct_discontinuity_error& e) {
        etiLog.level(warn) << e.what();
        uwd->failed_due_to_fct = true;
    }

    uwd->running = false;
    uwd->sync_barrier.get()->wait();
    etiLog.level(warn) << "UHD worker terminated";
}

// Check function for GPS fixtype
bool check_gps_fix_ok(struct UHDWorkerData *uwd)
{
    try {
        std::string fixtype(
                uwd->myUsrp->get_mboard_sensor("gps_fixtype", 0).to_pp_string());

        if (fixtype.find("3d fix") == std::string::npos) {
            etiLog.level(warn) << "OutputUHD: " << fixtype;

            return false;
        }

        return true;
    }
    catch (uhd::lookup_error &e) {
        etiLog.level(warn) << "OutputUHD: no gps_fixtype sensor";
        return false;
    }
}

void UHDWorker::process()
{
    int workerbuffer = 0;
    tx_second        = 0;
    pps_offset       = 0.0;
    last_pps         = 2.0;

    // Variables needed for GPS fix check
    last_gps_fix_check = 0.0;
    num_checks_without_gps_fix = 0;

#if FAKE_UHD == 0
    uhd::stream_args_t stream_args("fc32"); //complex floats
    myTxStream = uwd->myUsrp->get_tx_stream(stream_args);
#endif

    md.start_of_burst = false;
    md.end_of_burst   = false;

    expected_next_fct = -1;

    num_underflows   = 0;
    num_late_packets = 0;

    while (uwd->running) {
        fct_discontinuity = false;
        md.has_time_spec  = false;
        md.time_spec      = uhd::time_spec_t(0.0);

        /* Wait for barrier */
        // this wait will hopefully always be the second one
        // because modulation should be quicker than transmission
        uwd->sync_barrier.get()->wait();

        struct UHDWorkerFrameData* frame;

        if (workerbuffer == 0) {
            frame = &(uwd->frame0);
        }
        else if (workerbuffer == 1) {
            frame = &(uwd->frame1);
        }
        else {
            throw std::runtime_error(
                    "UHDWorker.process: workerbuffer is neither 0 nor 1 !");
        }

        handle_frame(frame);

        // swap buffers
        workerbuffer = (workerbuffer + 1) % 2;
    }
}

void UHDWorker::handle_frame(const struct UHDWorkerFrameData *frame)
{
    const double gps_fix_check_interval = 10.0; // seconds

    pps_offset = frame->ts.timestamp_pps_offset;

    // Tx second from MNSC
    tx_second = frame->ts.timestamp_sec;

    /* Verify that the FCT value is correct. If we miss one transmission
     * frame we must interrupt UHD and resync to the timestamps
     */
    if (frame->ts.fct == -1) {
        etiLog.level(info) <<
            "OutputUHD: dropping one frame with invalid FCT";
        return;
    }
    if (expected_next_fct != -1) {
        if (expected_next_fct != (int)frame->ts.fct) {
            etiLog.level(warn) <<
                "OutputUHD: Incorrect expect fct " << frame->ts.fct <<
                ", expected " << expected_next_fct;

            fct_discontinuity = true;
            throw fct_discontinuity_error();
        }
    }

    expected_next_fct = (frame->ts.fct + uwd->fct_increment) % 250;

    // Check for ref_lock
    if (uwd->check_refclk_loss) {
        try {
            // TODO: Is this check specific to the B100 and USRP2 ?
            if (! uwd->myUsrp->get_mboard_sensor("ref_locked", 0).to_bool()) {
                etiLog.log(alert,
                        "OutputUHD: External reference clock lock lost !");
                if (uwd->refclk_lock_loss_behaviour == CRASH) {
                    throw std::runtime_error(
                            "OutputUHD: External reference clock lock lost.");
                }
            }
        }
        catch (uhd::lookup_error &e) {
            uwd->check_refclk_loss = false;
            etiLog.log(warn,
                    "OutputUHD: This USRP does not have mboard sensor for ext clock loss."
                    " Check disabled.");
        }
    }

    double usrp_time = uwd->myUsrp->get_time_now().get_real_secs();

    if (uwd->check_gpsfix and
            // Divide interval by two because we alternate between
            // launch and check
            last_gps_fix_check + gps_fix_check_interval/2.0 < usrp_time) {
        last_gps_fix_check = usrp_time;

        // Alternate between launching thread and checking the
        // result.
        if (gps_fix_task.joinable()) {
            if (gps_fix_future.has_value()) {

                gps_fix_future.wait();

                gps_fix_task.join();

                if (not gps_fix_future.get()) {
                    if (num_checks_without_gps_fix == 0) {
                        etiLog.level(alert) <<
                            "OutputUHD: GPS Fix lost";
                    }
                    num_checks_without_gps_fix++;
                }
                else {
                    if (num_checks_without_gps_fix) {
                        etiLog.level(info) <<
                            "OutputUHD: GPS Fix recovered";
                    }
                    num_checks_without_gps_fix = 0;
                }

                if (gps_fix_check_interval * num_checks_without_gps_fix >
                        uwd->max_gps_holdover) {
                    std::stringstream ss;
                    ss << "Lost GPS fix for " << gps_fix_check_interval *
                        num_checks_without_gps_fix << " seconds";
                    throw std::runtime_error(ss.str());
                }
            }
        }
        else {
            // Checking the sensor here takes too much
            // time, it has to be done in a separate thread.
            gps_fix_pt = boost::packaged_task<bool>(
                    boost::bind(check_gps_fix_ok, uwd) );

            gps_fix_future = gps_fix_pt.get_future();

            gps_fix_task = boost::thread(boost::move(gps_fix_pt));
        }
    }


    if (uwd->sourceContainsTimestamp) {
        if (!frame->ts.timestamp_valid) {
            /* We have not received a full timestamp through
             * MNSC. We sleep through the frame.
             */
            etiLog.level(info) <<
                "OutputUHD: Throwing sample " << frame->ts.fct <<
                " away: incomplete timestamp " << tx_second <<
                " + " << pps_offset;
            usleep(20000); //TODO should this be TM-dependant ?
            return;
        }

        md.has_time_spec = true;
        md.time_spec = uhd::time_spec_t(tx_second, pps_offset);

        // md is defined, let's do some checks
        if (md.time_spec.get_real_secs() + tx_timeout < usrp_time) {
            etiLog.level(warn) <<
                "OutputUHD: Timestamp in the past! offset: " <<
                md.time_spec.get_real_secs() - usrp_time <<
                "  (" << usrp_time << ")"
                " frame " << frame->ts.fct <<
                ", tx_second " << tx_second <<
                ", pps " << pps_offset;
            return;
        }

        if (md.time_spec.get_real_secs() > usrp_time + TIMESTAMP_ABORT_FUTURE) {
            etiLog.level(error) <<
                "OutputUHD: Timestamp way too far in the future! offset: " <<
                md.time_spec.get_real_secs() - usrp_time;
            throw std::runtime_error("Timestamp error. Aborted.");
        }
    }
    else { // !uwd->sourceContainsTimestamp
        if (uwd->muting || uwd->muteNoTimestamps) {
            /* There was some error decoding the timestamp
            */
            if (uwd->muting) {
                etiLog.log(info,
                        "OutputUHD: Muting sample %d requested\n",
                        frame->ts.fct);
            }
            else {
                etiLog.log(info,
                        "OutputUHD: Muting sample %d : no timestamp\n",
                        frame->ts.fct);
            }
            usleep(20000);
            return;
        }
    }

    tx_frame(frame);

    if (last_pps > pps_offset) {
        if (num_underflows or num_late_packets) {
            etiLog.log(info,
                    "OutputUHD status (usrp time: %f): "
                    "%d underruns and %d late packets since last status.\n",
                    usrp_time,
                    num_underflows, num_late_packets);
        }
        num_underflows = 0;
        num_late_packets = 0;
    }

    last_pps = pps_offset;
}

void UHDWorker::tx_frame(const struct UHDWorkerFrameData *frame)
{
    const size_t sizeIn = uwd->bufsize / sizeof(complexf);
    const complexf* in_data = reinterpret_cast<const complexf*>(frame->buf);

#if FAKE_UHD == 0
    size_t usrp_max_num_samps = myTxStream->get_max_num_samps();
#else
    size_t usrp_max_num_samps = 2048; // arbitrarily chosen
#endif

    size_t num_acc_samps = 0; //number of accumulated samples
    while (uwd->running && !uwd->muting && (num_acc_samps < sizeIn)) {
        size_t samps_to_send = std::min(sizeIn - num_acc_samps, usrp_max_num_samps);

        //ensure the the last packet has EOB set if the timestamps has been
        //refreshed and need to be reconsidered.
        //Also, if we saw that the FCT did not increment as expected, which
        //could be due to a lost incoming packet.
        md.end_of_burst = (
                uwd->sourceContainsTimestamp &&
                (frame->ts.timestamp_refresh || fct_discontinuity) &&
                samps_to_send <= usrp_max_num_samps );


#if FAKE_UHD
        // This is probably very approximate
        usleep( (1000000 / uwd->sampleRate) * samps_to_send);
        size_t num_tx_samps = samps_to_send;
#else
        //send a single packet
        size_t num_tx_samps = myTxStream->send(
                &in_data[num_acc_samps],
                samps_to_send, md, tx_timeout);
#endif

        num_acc_samps += num_tx_samps;

        md.time_spec = uhd::time_spec_t(tx_second, pps_offset)
            + uhd::time_spec_t(0, num_acc_samps/uwd->sampleRate);

        if (num_tx_samps == 0) {
            etiLog.log(warn,
                    "UHDWorker::process() unable to write to device, skipping frame!\n");
            break;
        }

        print_async_metadata(frame);
    }
}

void UHDWorker::print_async_metadata(const struct UHDWorkerFrameData *frame)
{
#if FAKE_UHD == 0
    uhd::async_metadata_t async_md;
    if (uwd->myUsrp->get_device()->recv_async_msg(async_md, 0)) {
        const char* uhd_async_message = "";
        bool failure = false;
        switch (async_md.event_code) {
            case uhd::async_metadata_t::EVENT_CODE_BURST_ACK:
                break;
            case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW:
                uhd_async_message = "Underflow";
                num_underflows++;
                break;
            case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR:
                uhd_async_message = "Packet loss between host and device.";
                failure = true;
                break;
            case uhd::async_metadata_t::EVENT_CODE_TIME_ERROR:
                uhd_async_message = "Packet had time that was late.";
                num_late_packets++;
                break;
            case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET:
                uhd_async_message = "Underflow occurred inside a packet.";
                failure = true;
                break;
            case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR_IN_BURST:
                uhd_async_message = "Packet loss within a burst.";
                failure = true;
                break;
            default:
                uhd_async_message = "unknown event code";
                failure = true;
                break;
        }

        if (failure) {
            etiLog.level(alert) << "Near frame " <<
                frame->ts.fct << ": Received Async UHD Message '" << 
                uhd_async_message << "'";

        }
    }
#endif
}


void OutputUHD::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "txgain") {
        ss >> myConf.txgain;
        myUsrp->set_tx_gain(myConf.txgain);
    }
    else if (parameter == "freq") {
        ss >> myConf.frequency;
        myUsrp->set_tx_freq(myConf.frequency);
        myConf.frequency = myUsrp->get_tx_freq();
    }
    else if (parameter == "muting") {
        ss >> myMuting;
    }
    else if (parameter == "staticdelay") {
        int64_t adjust;
        ss >> adjust;
        if (adjust > (myTFDurationMs * 1000))
        { // reset static delay for values outside range
            myStaticDelayUs = 0;
        }
        else
        { // the new adjust value is added to the existing delay and the result
            // is wrapped around at TF duration
            int newStaticDelayUs = myStaticDelayUs + adjust;
            if (newStaticDelayUs > (myTFDurationMs * 1000))
                myStaticDelayUs = newStaticDelayUs - (myTFDurationMs * 1000);
            else if (newStaticDelayUs < 0)
                myStaticDelayUs = newStaticDelayUs + (myTFDurationMs * 1000);
            else
                myStaticDelayUs = newStaticDelayUs;
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string OutputUHD::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "txgain") {
        ss << myConf.txgain;
    }
    else if (parameter == "freq") {
        ss << myConf.frequency;
    }
    else if (parameter == "muting") {
        ss << myMuting;
    }
    else if (parameter == "staticdelay") {
        ss << myStaticDelayUs;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

#endif // HAVE_OUTPUT_UHD

