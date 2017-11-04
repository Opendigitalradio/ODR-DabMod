/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "output/UHD.h"

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

namespace Output {

// Maximum number of frames that can wait in frames
static const size_t FRAMES_MAX_SIZE = 8;

static std::string stringtrim(const std::string &s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(),
            [](int c){ return std::isspace(c);} );
    return std::string(wsfront,
            std::find_if_not(s.rbegin(),
                std::string::const_reverse_iterator(wsfront),
                [](int c){ return std::isspace(c);} ).base());
}

static void uhd_msg_handler(uhd::msg::type_t type, const std::string &msg)
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
static bool check_gps_timelock(uhd::usrp::multi_usrp::sptr usrp)
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
static bool check_gps_locked(uhd::usrp::multi_usrp::sptr usrp)
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


UHD::UHD(
        SDRDeviceConfig& config) :
    SDRDevice(),
    m_conf(config),
    m_running(false)
{
    // Variables needed for GPS fix check
    first_gps_fix_check.tv_sec = 0;
    last_gps_fix_check.tv_sec = 0;
    time_last_frame.tv_sec = 0;

    std::stringstream device;
    device << m_conf.device;

    if (m_conf.masterClockRate != 0) {
        if (device.str() != "") {
            device << ",";
        }
        device << "master_clock_rate=" << m_conf.masterClockRate;
    }

    MDEBUG("OutputUHD::OutputUHD(device: %s) @ %p\n",
            device.str().c_str(), this);

    /* TODO
    RC_ADD_PARAMETER(rxgain, "UHD analog daughterboard RX gain for DPD feedback");
    */

    uhd::msg::register_handler(uhd_msg_handler);

    uhd::set_thread_priority_safe();

    etiLog.log(info, "OutputUHD:Creating the usrp device with: %s...",
            device.str().c_str());

    m_usrp = uhd::usrp::multi_usrp::make(device.str());

    etiLog.log(info, "OutputUHD:Using device: %s...",
            m_usrp->get_pp_string().c_str());

    if (m_conf.masterClockRate != 0.0) {
        double master_clk_rate = m_usrp->get_master_clock_rate();
        etiLog.log(debug, "OutputUHD:Checking master clock rate: %f...",
                master_clk_rate);

        if (fabs(master_clk_rate - m_conf.masterClockRate) >
                (m_conf.masterClockRate * 1e-6)) {
            throw std::runtime_error("Cannot set USRP master_clock_rate. Aborted.");
        }
    }

    MDEBUG("OutputUHD:Setting REFCLK and PPS input...\n");

    if (m_conf.refclk_src == "gpsdo-ettus") {
        m_usrp->set_clock_source("gpsdo");
    }
    else {
        m_usrp->set_clock_source(m_conf.refclk_src);
    }
    m_usrp->set_time_source(m_conf.pps_src);

    if (m_conf.subDevice != "") {
        m_usrp->set_tx_subdev_spec(uhd::usrp::subdev_spec_t(m_conf.subDevice),
                uhd::usrp::multi_usrp::ALL_MBOARDS);
    }

    etiLog.level(debug) << "UHD clock source is " << m_usrp->get_clock_source(0);

    etiLog.level(debug) << "UHD time source is " << m_usrp->get_time_source(0);

    m_usrp->set_tx_rate(m_conf.sampleRate);
    etiLog.log(debug, "OutputUHD:Set rate to %d. Actual TX Rate: %f sps...",
            m_conf.sampleRate, m_usrp->get_tx_rate());

    if (fabs(m_usrp->get_tx_rate() / m_conf.sampleRate) >
             m_conf.sampleRate * 1e-6) {
        throw std::runtime_error("Cannot set USRP sample rate. Aborted.");
    }

    tune(m_conf.lo_offset, m_conf.frequency);

    m_conf.frequency = m_usrp->get_tx_freq();
    etiLog.level(info) << std::fixed << std::setprecision(3) <<
        "OutputUHD:Actual TX frequency: " << m_conf.frequency;

    etiLog.level(info) << std::fixed << std::setprecision(3) <<
        "OutputUHD:Actual RX frequency: " << m_usrp->get_tx_freq();

    m_usrp->set_tx_gain(m_conf.txgain);
    m_conf.txgain = m_usrp->get_tx_gain();
    etiLog.log(debug, "OutputUHD:Actual TX Gain: %f", m_conf.txgain);

    etiLog.log(debug, "OutputUHD:Mute on missing timestamps: %s",
            m_conf.muteNoTimestamps ? "enabled" : "disabled");

    // preparing output thread worker data
    // TODO sourceContainsTimestamp = false;

    m_usrp->set_rx_rate(m_conf.sampleRate);
    etiLog.log(debug, "OutputUHD:Actual RX Rate: %f sps.", m_usrp->get_rx_rate());

    m_usrp->set_rx_antenna("RX2");
    etiLog.log(debug, "OutputUHD:Set RX Antenna: %s",
            m_usrp->get_rx_antenna().c_str());

    m_usrp->set_rx_gain(m_conf.rxgain);
    etiLog.log(debug, "OutputUHD:Actual RX Gain: %f", m_usrp->get_rx_gain());

    /* TODO
    uhdFeedback = std::make_shared<OutputUHDFeedback>(
            m_usrp, m_conf.dpdFeedbackServerPort, m_conf.sampleRate);
    */

    const uhd::stream_args_t stream_args("fc32"); //complex floats
    m_rx_stream = m_usrp->get_rx_stream(stream_args);
    m_tx_stream = m_usrp->get_tx_stream(stream_args);

    MDEBUG("OutputUHD:UHD ready.\n");
}

UHD::~UHD()
{
    stop_threads();
}

void UHD::tune(double lo_offset, double frequency)
{
    if (lo_offset != 0.0) {
        etiLog.level(info) << std::fixed << std::setprecision(3) <<
            "OutputUHD:Setting freq to " << frequency <<
            "  with LO offset " << lo_offset << "...";

        const auto tr = uhd::tune_request_t(frequency, lo_offset);
        uhd::tune_result_t result = m_usrp->set_tx_freq(tr);

        etiLog.level(debug) << "OutputUHD:" <<
            std::fixed << std::setprecision(0) <<
            " Target RF: " << result.target_rf_freq <<
            " Actual RF: " << result.actual_rf_freq <<
            " Target DSP: " << result.target_dsp_freq <<
            " Actual DSP: " << result.actual_dsp_freq;
    }
    else {
        //set the centre frequency
        etiLog.level(info) << std::fixed << std::setprecision(3) <<
            "OutputUHD:Setting freq to " << frequency << "...";
        m_usrp->set_tx_freq(frequency);
    }

    // TODO configure LO offset also for RX
    m_usrp->set_rx_freq(frequency);
}

double UHD::get_tx_freq(void)
{
    return m_usrp->get_tx_freq();
}

void UHD::set_txgain(double txgain)
{
    m_usrp->set_tx_gain(txgain);
    m_conf.txgain = m_usrp->get_tx_gain();
}

double UHD::get_txgain(void)
{
    return m_usrp->get_tx_gain();
}

void UHD::transmit_frame(const struct FrameData& frame)
{
    const double tx_timeout = 20.0;
    const size_t sizeIn = frame.buf.size() / sizeof(complexf);
    const complexf* in_data = reinterpret_cast<const complexf*>(&frame.buf[0]);

    size_t usrp_max_num_samps = m_tx_stream->get_max_num_samps();
    size_t num_acc_samps = 0; //number of accumulated samples
    while (m_running.load() and (not m_conf.muting) and (num_acc_samps < sizeIn)) {
        size_t samps_to_send = std::min(sizeIn - num_acc_samps, usrp_max_num_samps);

        uhd::tx_metadata_t md_tx = md;

        // ensure the the last packet has EOB set if the timestamps has been
        // refreshed and need to be reconsidered.
        md_tx.end_of_burst = (
                frame.ts.timestamp_valid and
                frame.ts.timestamp_refresh and
                samps_to_send <= usrp_max_num_samps );

        //send a single packet
        size_t num_tx_samps = m_tx_stream->send(
                &in_data[num_acc_samps],
                samps_to_send, md_tx, tx_timeout);
        etiLog.log(trace, "UHD,sent %zu of %zu", num_tx_samps, samps_to_send);

        num_acc_samps += num_tx_samps;

        md_tx.time_spec = md.time_spec +
            uhd::time_spec_t(0, num_tx_samps/m_conf.sampleRate);

        if (num_tx_samps == 0) {
            etiLog.log(warn,
                    "OutputUHD unable to write to device, skipping frame!");
            break;
        }
    }
}


SDRDevice::RunStatistics UHD::get_run_statistics(void)
{
    RunStatistics rs;
    rs.num_underruns = num_underflows;
    rs.num_overruns = num_overflows;
    rs.num_late_packets = num_late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}

double UHD::get_real_secs(void)
{
    return m_usrp->get_time_now().get_real_secs();
}

void UHD::set_rxgain(double rxgain)
{
    m_usrp->set_rx_gain(m_conf.rxgain);
    m_conf.rxgain = m_usrp->get_rx_gain();
}

double UHD::get_rxgain()
{
    return m_usrp->get_rx_gain();
}

size_t UHD::receive_frame(
        complexf *buf,
        size_t num_samples,
        struct frame_timestamp& ts,
        double timeout_secs)
{
    uhd::stream_cmd_t cmd(
            uhd::stream_cmd_t::stream_mode_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    cmd.num_samps = num_samples;
    cmd.stream_now = false;
    cmd.time_spec = uhd::time_spec_t(ts.timestamp_sec, ts.pps_offset());

    m_rx_stream->issue_stream_cmd(cmd);

    uhd::rx_metadata_t md;

    constexpr double timeout = 60;
    size_t samples_read = m_rx_stream->recv(buf, num_samples, md, timeout);

    // Update the ts with the effective receive TS
    ts.timestamp_sec = md.time_spec.get_full_secs();
    ts.timestamp_pps = md.time_spec.get_frac_secs() * 16384000.0;
    return samples_read;
}

// Return true if GPS and reference clock inputs are ok
bool UHD::is_clk_source_ok(void)
{
    //TODO
    return true;
}

const char* UHD::device_name(void)
{
    return "UHD";
}


bool UHD::refclk_loss_needs_check() const
{
    if (suppress_refclk_loss_check) {
        return false;
    }
    return m_conf.refclk_src != "internal";
}

bool UHD::gpsfix_needs_check() const
{
    if (m_conf.refclk_src == "internal") {
        return false;
    }
    else if (m_conf.refclk_src == "gpsdo") {
        return (m_conf.maxGPSHoldoverTime != 0);
    }
    else if (m_conf.refclk_src == "gpsdo-ettus") {
        return (m_conf.maxGPSHoldoverTime != 0);
    }
    else {
        return false;
    }
}

bool UHD::gpsdo_is_ettus() const
{
    return (m_conf.refclk_src == "gpsdo-ettus");
}

void UHD::stop_threads()
{
    m_running.store(false);
    if (m_async_rx_thread.joinable()) {
        m_async_rx_thread.join();
    }
}


static int transmission_frame_duration_ms(unsigned int dabMode)
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


void UHD::set_usrp_time()
{
    if (m_conf.enableSync and (m_conf.pps_src == "none")) {
        etiLog.level(warn) <<
            "OutputUHD: WARNING:"
            " you are using synchronous transmission without PPS input!";

        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now)) {
            perror("OutputUHD:Error: could not get time: ");
            etiLog.level(error) << "OutputUHD: could not get time";
        }
        else {
            m_usrp->set_time_now(uhd::time_spec_t(now.tv_sec));
            etiLog.level(info) << "OutputUHD: Setting USRP time to " <<
                std::fixed <<
                uhd::time_spec_t(now.tv_sec).get_real_secs();
        }
    }

    if (m_conf.pps_src != "none") {
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
            m_usrp->set_time_unknown_pps(uhd::time_spec_t(seconds + 2));
            etiLog.level(info) << "OutputUHD: Setting USRP time next pps to " <<
                std::fixed <<
                uhd::time_spec_t(seconds + 2).get_real_secs();
        }

        usleep(1e6);
        etiLog.log(info,  "OutputUHD: USRP time %f\n",
                m_usrp->get_time_now().get_real_secs());
    }
}

void UHD::initial_gps_check()
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
        ss << "GPS did not show time lock in " <<
            initial_gps_fix_wait << " seconds";
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
    long wait_time_us = transmission_frame_duration_ms(m_conf.dabMode);

    if (wait_time_us - delta_us > 0) {
        usleep(wait_time_us - delta_us);
    }

    time_last_frame.tv_nsec += wait_time_us * 1000;
    if (time_last_frame.tv_nsec >= 1000000000L) {
        time_last_frame.tv_nsec -= 1000000000L;
        time_last_frame.tv_sec++;
    }
}

void UHD::check_gps()
{
    struct timespec time_now;
    if (clock_gettime(CLOCK_MONOTONIC, &time_now) != 0) {
        stringstream ss;
        ss << "clock_gettime failure: " << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    // Divide interval by two because we alternate between
    // launch and check
    if (gpsfix_needs_check() and
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
                        m_conf.maxGPSHoldoverTime) {
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
            if (gpsdo_is_ettus()) {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_locked, m_usrp) );
            }
            else {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_timelock, m_usrp) );
            }
            gps_fix_future = gps_fix_pt.get_future();

            gps_fix_task = boost::thread(boost::move(gps_fix_pt));
        }
    }
}

void UHD::print_async_thread()
{
    while (m_running.load()) {
        uhd::async_metadata_t async_md;
        if (m_usrp->get_device()->recv_async_msg(async_md, 1)) {
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
                etiLog.level(alert) <<
                    "Received Async UHD Message '" <<
                    uhd_async_message << "' at time " <<
                    md.time_spec.get_real_secs();
            }
        }

        auto time_now = std::chrono::steady_clock::now();
        if (last_print_time + std::chrono::seconds(1) < time_now) {
            const double usrp_time =
                m_usrp->get_time_now().get_real_secs();

            if ( (num_underflows > num_underflows_previous) or
                 (num_late_packets > num_late_packets_previous)) {
                etiLog.log(info,
                        "OutputUHD status (usrp time: %f): "
                        "%d underruns and %d late packets since last status.\n",
                        usrp_time,
                        num_underflows, num_late_packets);
            }

            num_underflows_previous = num_underflows;
            num_late_packets_previous = num_late_packets;

            last_print_time = time_now;
        }
    }
}

} // namespace Output

#endif // HAVE_OUTPUT_UHD

