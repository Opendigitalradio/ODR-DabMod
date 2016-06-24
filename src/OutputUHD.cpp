/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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
#include "Utils.h"

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
#include <pthread.h>

using namespace std;

typedef std::complex<float> complexf;

std::string stringtrim(const std::string &s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){return std::isspace(c);} );
    return std::string(wsfront, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(wsfront), [](int c){return std::isspace(c);} ).base());
}

void uhd_msg_handler(uhd::msg::type_t type, const std::string &msg)
{
    if (type == uhd::msg::warning) {
        etiLog.level(warn) << "UHD Warning: " << msg;
    }
    else if (type == uhd::msg::error) {
        etiLog.level(error) << "UHD Error: " << msg;
    }
    else {
        // do not print very short U messages and such
        if (stringtrim(msg).size() != 1) {
            etiLog.level(debug) << "UHD Message: " << msg;
        }
    }
}

// Check function for GPS TIMELOCK sensor from the ODR LEA-M8F board GPSDO
bool check_gps_timelock(uhd::usrp::multi_usrp::sptr usrp)
{
    try {
        std::string sensor_value(
                usrp->get_mboard_sensor("gps_timelock", 0).to_pp_string());

        if (sensor_value.find("TIME LOCKED") == std::string::npos) {
            etiLog.level(warn) << "OutputUHD: gps_timelock " << sensor_value;
            return false;
        }

        return true;
    }
    catch (uhd::lookup_error &e) {
        etiLog.level(warn) << "OutputUHD: no gps_timelock sensor";
        return false;
    }
}

// Check function for GPS LOCKED sensor from the Ettus GPSDO
bool check_gps_locked(uhd::usrp::multi_usrp::sptr usrp)
{
    try {
        uhd::sensor_value_t sensor_value(
                usrp->get_mboard_sensor("gps_locked", 0));
        if (not sensor_value.to_bool()) {
            etiLog.level(warn) << "OutputUHD: gps_locked " <<
                sensor_value.to_pp_string();
            return false;
        }

        return true;
    }
    catch (uhd::lookup_error &e) {
        etiLog.level(warn) << "OutputUHD: no gps_locked sensor";
        return false;
    }
}


OutputUHD::OutputUHD(
        OutputUHDConfig& config) :
    ModOutput(ModFormat(1), ModFormat(0)),
    RemoteControllable("uhd"),
    myConf(config),
    // Since we don't know the buffer size, we cannot initialise
    // the buffers at object initialisation.
    first_run(true),
    gps_fix_verified(false),
    worker(&uwd),
    myDelayBuf(0)
{
    myConf.muting = true;     // is remote-controllable, and reset by the GPS fix check
    myConf.staticDelayUs = 0; // is remote-controllable

    // Variables needed for GPS fix check
    num_checks_without_gps_fix = 1;
    first_gps_fix_check.tv_sec = 0;
    last_gps_fix_check.tv_sec = 0;
    time_last_frame.tv_sec = 0;


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

    if (myConf.refclk_src == "gpsdo-ettus") {
        myUsrp->set_clock_source("gpsdo");
    }
    else {
        myUsrp->set_clock_source(myConf.refclk_src);
    }
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

    // preparing output thread worker data
    uwd.myUsrp = myUsrp;
#endif

    uwd.sampleRate = myConf.sampleRate;
    uwd.sourceContainsTimestamp = false;
    uwd.muteNoTimestamps = myConf.muteNoTimestamps;
    uwd.refclk_lock_loss_behaviour = myConf.refclk_lock_loss_behaviour;
    uwd.gpsdo_is_ettus = false;

    if (myConf.refclk_src == "internal") {
        uwd.check_refclk_loss = false;
        uwd.check_gpsfix = false;
    }
    else if (myConf.refclk_src == "gpsdo") {
        uwd.check_refclk_loss = true;
        uwd.check_gpsfix = (myConf.maxGPSHoldoverTime != 0);
    }
    else if (myConf.refclk_src == "gpsdo-ettus") {
        uwd.check_refclk_loss = true;
        uwd.check_gpsfix = (myConf.maxGPSHoldoverTime != 0);
        uwd.gpsdo_is_ettus = true;
    }
    else {
        uwd.check_refclk_loss = true;
        uwd.check_gpsfix = false;
    }

    SetDelayBuffer(myConf.dabMode);

    MDEBUG("OutputUHD:UHD ready.\n");
}


OutputUHD::~OutputUHD()
{
    MDEBUG("OutputUHD::~OutputUHD() @ %p\n", this);
}


void OutputUHD::setETIReader(EtiReader *etiReader)
{
    myEtiReader = etiReader;
}

int transmission_frame_duration_ms(unsigned int dabMode)
{
    switch (dabMode) {
        // could happen when called from constructor and we take the mode from ETI
        case 0: return 0;

        case 1: return 96;
        case 2: return 24;
        case 3: return 24;
        case 4: return 48;
        default:
            throw std::runtime_error("OutputUHD: invalid DAB mode");
    }
}

void OutputUHD::SetDelayBuffer(unsigned int dabMode)
{
    // find out the duration of the transmission frame (Table 2 in ETSI 300 401)
    myTFDurationMs = transmission_frame_duration_ms(dabMode);

    // The buffer size equals the number of samples per transmission frame so
    // we calculate it by multiplying the duration of the transmission frame
    // with the samplerate.
    myDelayBuf.resize(myTFDurationMs * myConf.sampleRate / 1000);
}

int OutputUHD::process(Buffer* dataIn, Buffer* dataOut)
{
    uwd.muting = myConf.muting;

    if (not gps_fix_verified) {
        if (uwd.check_gpsfix) {
            initial_gps_check();

            if (num_checks_without_gps_fix == 0) {
                set_usrp_time();
                gps_fix_verified = true;
                myConf.muting = false;
            }
        }
        else {
            set_usrp_time();
            gps_fix_verified = true;
            myConf.muting = false;
        }
    }
    else {
        if (first_run) {
            etiLog.level(debug) << "OutputUHD: UHD initialising...";

            // we only set the delay buffer from the dab mode signaled in ETI if the
            // dab mode was not set in contructor
            if (myTFDurationMs == 0) {
                SetDelayBuffer(myEtiReader->getMode());
            }

            worker.start(&uwd);

            lastLen = dataIn->getLength();
            first_run = false;
            etiLog.level(debug) << "OutputUHD: UHD initialising complete";
        }

        if (lastLen != dataIn->getLength()) {
            // I expect that this never happens.
            etiLog.level(emerg) <<
                "OutputUHD: Fatal error, input length changed from " << lastLen <<
                " to " << dataIn->getLength();
            throw std::runtime_error("Non-constant input length!");
        }

        uwd.sourceContainsTimestamp = myConf.enableSync &&
            myEtiReader->sourceContainsTimestamp();


        if (uwd.check_gpsfix) {
            try {
                check_gps();
            }
            catch (std::runtime_error& e) {
                uwd.running = false;
                etiLog.level(error) << e.what();
            }
        }

        // Prepare the frame for the worker
        UHDWorkerFrameData frame;
        frame.buf.resize(dataIn->getLength());

        // calculate delay and fill buffer
        uint32_t noSampleDelay = (myConf.staticDelayUs * (myConf.sampleRate / 1000)) / 1000;
        uint32_t noByteDelay = noSampleDelay * sizeof(complexf);

        const uint8_t* pInData = (uint8_t*)dataIn->getData();

        uint8_t *pTmp = &frame.buf[0];
        if (noByteDelay) {
            // copy remain from delaybuf
            memcpy(pTmp, &myDelayBuf[0], noByteDelay);
            // copy new data
            memcpy(&pTmp[noByteDelay], pInData, dataIn->getLength() - noByteDelay);
            // copy remaining data to delay buf
            memcpy(&myDelayBuf[0], &pInData[dataIn->getLength() - noByteDelay], noByteDelay);
        }
        else {
            std::copy(pInData, pInData + dataIn->getLength(),
                    frame.buf.begin());
        }

        myEtiReader->calculateTimestamp(frame.ts);

        if (!uwd.running) {
            worker.stop();
            first_run = true;

            etiLog.level(error) <<
                "OutputUHD: Error, UHD worker failed";
            throw std::runtime_error("UHD worker failed");
        }

        if (frame.ts.fct == -1) {
            etiLog.level(info) <<
                "OutputUHD: dropping one frame with invalid FCT";
        }
        else {
            while (true) {
                size_t num_frames = uwd.frames.push_wait_if_full(frame,
                        FRAMES_MAX_SIZE);
                etiLog.log(trace, "UHD,push %zu", num_frames);
                break;
            }
        }
    }

    return dataIn->getLength();
}


void OutputUHD::set_usrp_time()
{
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
}

void OutputUHD::initial_gps_check()
{
    if (first_gps_fix_check.tv_sec == 0) {
        etiLog.level(info) << "Waiting for GPS fix";

        if (clock_gettime(CLOCK_MONOTONIC, &first_gps_fix_check) != 0) {
            stringstream ss;
            ss << "clock_gettime failure: " << strerror(errno);
            throw std::runtime_error(ss.str());
        }
    }

    check_gps();

    if (last_gps_fix_check.tv_sec >
            first_gps_fix_check.tv_sec + initial_gps_fix_wait) {
        stringstream ss;
        ss << "GPS did not show time lock in " << initial_gps_fix_wait << " seconds";
        throw std::runtime_error(ss.str());
    }

    if (time_last_frame.tv_sec == 0) {
        if (clock_gettime(CLOCK_MONOTONIC, &time_last_frame) != 0) {
            stringstream ss;
            ss << "clock_gettime failure: " << strerror(errno);
            throw std::runtime_error(ss.str());
        }
    }

    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        stringstream ss;
        ss << "clock_gettime failure: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    long delta_us = timespecdiff_us(time_last_frame, now);
    long wait_time_us = transmission_frame_duration_ms(myConf.dabMode);

    if (wait_time_us - delta_us > 0) {
        usleep(wait_time_us - delta_us);
    }

    time_last_frame.tv_nsec += wait_time_us * 1000;
    if (time_last_frame.tv_nsec >= 1000000000L) {
        time_last_frame.tv_nsec -= 1000000000L;
        time_last_frame.tv_sec++;
    }
}

void OutputUHD::check_gps()
{
    struct timespec time_now;
    if (clock_gettime(CLOCK_MONOTONIC, &time_now) != 0) {
        stringstream ss;
        ss << "clock_gettime failure: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    // Divide interval by two because we alternate between
    // launch and check
    if (uwd.check_gpsfix and
            last_gps_fix_check.tv_sec + gps_fix_check_interval/2.0 <
            time_now.tv_sec) {
        last_gps_fix_check = time_now;

        // Alternate between launching thread and checking the
        // result.
        if (gps_fix_task.joinable()) {
            if (gps_fix_future.has_value()) {

                gps_fix_future.wait();

                gps_fix_task.join();

                if (not gps_fix_future.get()) {
                    if (num_checks_without_gps_fix == 0) {
                        etiLog.level(alert) <<
                            "OutputUHD: GPS Time Lock lost";
                    }
                    num_checks_without_gps_fix++;
                }
                else {
                    if (num_checks_without_gps_fix) {
                        etiLog.level(info) <<
                            "OutputUHD: GPS Time Lock recovered";
                    }
                    num_checks_without_gps_fix = 0;
                }

                if (gps_fix_check_interval * num_checks_without_gps_fix >
                        myConf.maxGPSHoldoverTime) {
                    std::stringstream ss;
                    ss << "Lost GPS Time Lock for " << gps_fix_check_interval *
                        num_checks_without_gps_fix << " seconds";
                    throw std::runtime_error(ss.str());
                }
            }
        }
        else {
            // Checking the sensor here takes too much
            // time, it has to be done in a separate thread.
            if (uwd.gpsdo_is_ettus) {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_locked, myUsrp) );
            }
            else {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_timelock, myUsrp) );
            }
            gps_fix_future = gps_fix_pt.get_future();

            gps_fix_task = boost::thread(boost::move(gps_fix_pt));
        }
    }
}

//============================ UHD Worker ========================

void UHDWorker::process_errhandler()
{
    // Set thread priority to realtime
    if (int ret = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for UHD worker:" << ret;
    }

    set_thread_name("uhdworker");

    process();
    uwd->running = false;
    etiLog.level(warn) << "UHD worker terminated";
}

void UHDWorker::process()
{
    last_tx_time_initialised = false;

#if FAKE_UHD == 0
    uhd::stream_args_t stream_args("fc32"); //complex floats
    myTxStream = uwd->myUsrp->get_tx_stream(stream_args);
#endif

    md.start_of_burst = false;
    md.end_of_burst   = false;

    num_underflows   = 0;
    num_late_packets = 0;

    while (uwd->running) {
        md.has_time_spec  = false;
        md.time_spec      = uhd::time_spec_t(0.0);

        struct UHDWorkerFrameData frame;
        etiLog.log(trace, "UHD,wait");
        uwd->frames.wait_and_pop(frame);
        etiLog.log(trace, "UHD,pop");

        handle_frame(&frame);
    }
}

void UHDWorker::handle_frame(const struct UHDWorkerFrameData *frame)
{
    // Transmit timeout
    static const double tx_timeout = 20.0;

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
    bool timestamp_discontinuity = false;

    if (uwd->sourceContainsTimestamp) {
        // Tx time from MNSC and TIST
        uint32_t tx_second = frame->ts.timestamp_sec;
        uint32_t tx_pps    = frame->ts.timestamp_pps;

        if (!frame->ts.timestamp_valid) {
            /* We have not received a full timestamp through
             * MNSC. We sleep through the frame.
             */
            etiLog.level(info) <<
                "OutputUHD: Throwing sample " << frame->ts.fct <<
                " away: incomplete timestamp " << tx_second <<
                " / " << tx_pps;
            usleep(20000); //TODO should this be TM-dependant ?
            return;
        }

        if (last_tx_time_initialised) {
            const size_t sizeIn = frame->buf.size() / sizeof(complexf);
            uint64_t increment = (uint64_t)sizeIn * 16384000ul /
                                 (uint64_t)uwd->sampleRate;
                                  // samps  * ticks/s  / (samps/s)
                                  // (samps * ticks * s) / (s * samps)
                                  // ticks

            uint32_t expected_sec = last_tx_second + increment / 16384000ul;
            uint32_t expected_pps = last_tx_pps + increment % 16384000ul;

            while (expected_pps > 16384000) {
                expected_sec++;
                expected_pps -= 16384000;
            }

            if (expected_sec != tx_second or
                    expected_pps != tx_pps) {
                etiLog.level(warn) << "OutputUHD: timestamp irregularity!" <<
                    std::fixed <<
                    " Expected " << expected_sec << "+" <<
                    (double)expected_pps/16384000.0 <<
                    " Got " << tx_second << "+" <<
                    (double)tx_pps/16384000.0;

                timestamp_discontinuity = true;
            }
        }

        last_tx_second = tx_second;
        last_tx_pps    = tx_pps;
        last_tx_time_initialised = true;

        double pps_offset = tx_pps / 16384000.0;

        md.has_time_spec = true;
        md.time_spec = uhd::time_spec_t(tx_second, pps_offset);
        etiLog.log(trace, "UHD,tist %f", md.time_spec.get_real_secs());

        // md is defined, let's do some checks
        if (md.time_spec.get_real_secs() + tx_timeout < usrp_time) {
            etiLog.level(warn) <<
                "OutputUHD: Timestamp in the past! offset: " <<
                std::fixed <<
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

    tx_frame(frame, timestamp_discontinuity);

    auto time_now = std::chrono::steady_clock::now();
    if (last_print_time + std::chrono::seconds(1) < time_now) {
        if (num_underflows or num_late_packets) {
            etiLog.log(info,
                    "OutputUHD status (usrp time: %f): "
                    "%d underruns and %d late packets since last status.\n",
                    usrp_time,
                    num_underflows, num_late_packets);
        }
        num_underflows = 0;
        num_late_packets = 0;

        last_print_time = time_now;
    }
}

void UHDWorker::tx_frame(const struct UHDWorkerFrameData *frame, bool ts_update)
{
    const double tx_timeout = 20.0;
    const size_t sizeIn = frame->buf.size() / sizeof(complexf);
    const complexf* in_data = reinterpret_cast<const complexf*>(&frame->buf[0]);

#if FAKE_UHD == 0
    size_t usrp_max_num_samps = myTxStream->get_max_num_samps();
#else
    size_t usrp_max_num_samps = 2048; // arbitrarily chosen
#endif

    size_t num_acc_samps = 0; //number of accumulated samples
    while (uwd->running && !uwd->muting && (num_acc_samps < sizeIn)) {
        size_t samps_to_send = std::min(sizeIn - num_acc_samps, usrp_max_num_samps);

        uhd::tx_metadata_t md_tx = md;

        //ensure the the last packet has EOB set if the timestamps has been
        //refreshed and need to be reconsidered.
        md_tx.end_of_burst = (
                uwd->sourceContainsTimestamp &&
                (frame->ts.timestamp_refresh or ts_update) &&
                samps_to_send <= usrp_max_num_samps );


#if FAKE_UHD
        // This is probably very approximate
        usleep( (1000000 / uwd->sampleRate) * samps_to_send);
        size_t num_tx_samps = samps_to_send;
#else
        //send a single packet
        size_t num_tx_samps = myTxStream->send(
                &in_data[num_acc_samps],
                samps_to_send, md_tx, tx_timeout);
        etiLog.log(trace, "UHD,sent %zu of %zu", num_tx_samps, samps_to_send);
#endif

        num_acc_samps += num_tx_samps;

        md_tx.time_spec = md.time_spec +
            uhd::time_spec_t(0, num_tx_samps/uwd->sampleRate);

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

// =======================================
// Remote Control for UHD
// =======================================

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
        ss >> myConf.muting;
    }
    else if (parameter == "staticdelay") {
        int64_t adjust;
        ss >> adjust;
        if (adjust > (myTFDurationMs * 1000))
        { // reset static delay for values outside range
            myConf.staticDelayUs = 0;
        }
        else
        { // the new adjust value is added to the existing delay and the result
            // is wrapped around at TF duration
            int newStaticDelayUs = myConf.staticDelayUs + adjust;
            if (newStaticDelayUs > (myTFDurationMs * 1000))
                myConf.staticDelayUs = newStaticDelayUs - (myTFDurationMs * 1000);
            else if (newStaticDelayUs < 0)
                myConf.staticDelayUs = newStaticDelayUs + (myTFDurationMs * 1000);
            else
                myConf.staticDelayUs = newStaticDelayUs;
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
        ss << myConf.muting;
    }
    else if (parameter == "staticdelay") {
        ss << myConf.staticDelayUs;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

#endif // HAVE_OUTPUT_UHD

