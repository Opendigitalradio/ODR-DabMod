/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
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

//#define MDEBUG(fmt, args...) fprintf(LOG, fmt , ## args)
#define MDEBUG(fmt, args...)

#include "PcDebug.h"
#include "Log.h"
#include "RemoteControl.h"
#include "Utils.h"

#include <thread>
#include <iomanip>

#include <uhd/version.hpp>
// 3.11.0.0 introduces the API breaking change, where
// uhd::msg is replaced by the new log API
#if UHD_VERSION >= 3110000
# define UHD_HAS_LOG_API 1
# include <uhd/utils/log_add.hpp>
# include <uhd/utils/thread.hpp>
#else
# define UHD_HAS_LOG_API 0
# include <uhd/utils/msg.hpp>
# include <uhd/utils/thread_priority.hpp>
#endif


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

#if UHD_HAS_LOG_API == 1
static void uhd_log_handler(const uhd::log::logging_info& info)
{
    // do not print very short U messages, nor those of
    // verbosity trace or debug
    if (info.verbosity >= uhd::log::info and
            stringtrim(info.message).size() != 1) {
        etiLog.level(debug) << "UHD Message (" <<
            (int)info.verbosity << ") " <<
            info.component << ": " << info.message;
    }
}
#else
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
#endif // UHD_HAS_LOG_API



UHD::UHD(SDRDeviceConfig& config) :
    SDRDevice(),
    m_conf(config),
    m_running(false)
{
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

#if UHD_HAS_LOG_API == 1
    uhd::log::add_logger("dabmod", uhd_log_handler);
    try {
        uhd::log::set_console_level(uhd::log::fatal);
    }
    catch (const uhd::key_error&) {
        etiLog.level(warn) << "OutputUHD: Could not set UHD console loglevel";
    }
#else
    uhd::msg::register_handler(uhd_msg_handler);
#endif

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

    m_device_time = std::make_shared<USRPTime>(m_usrp, m_conf);

    m_usrp->set_tx_rate(m_conf.sampleRate);
    etiLog.log(debug, "OutputUHD:Set rate to %d. Actual TX Rate: %f sps...",
            m_conf.sampleRate, m_usrp->get_tx_rate());

    if (fabs(m_usrp->get_tx_rate() / m_conf.sampleRate) >
             m_conf.sampleRate * 1e-6) {
        throw std::runtime_error("Cannot set USRP sample rate. Aborted.");
    }

    if (m_conf.bandwidth > 0) {
        m_usrp->set_tx_bandwidth(m_conf.bandwidth);
        m_usrp->set_rx_bandwidth(m_conf.bandwidth);

        etiLog.level(info) << "OutputUHD:Actual TX bandwidth: " <<
            std::fixed << std::setprecision(2) <<
            m_usrp->get_tx_bandwidth();
    }

    tune(m_conf.lo_offset, m_conf.frequency);

    m_conf.frequency = m_usrp->get_tx_freq();
    etiLog.level(debug) << std::fixed << std::setprecision(3) <<
        "OutputUHD:Actual TX frequency: " << m_conf.frequency;

    etiLog.level(debug) << std::fixed << std::setprecision(3) <<
        "OutputUHD:Actual RX frequency: " << m_usrp->get_tx_freq();

    m_usrp->set_tx_gain(m_conf.txgain);
    m_conf.txgain = m_usrp->get_tx_gain();
    etiLog.log(debug, "OutputUHD:Actual TX Gain: %f", m_conf.txgain);

    etiLog.log(debug, "OutputUHD:Mute on missing timestamps: %s",
            m_conf.muteNoTimestamps ? "enabled" : "disabled");

    m_usrp->set_rx_rate(m_conf.sampleRate);
    etiLog.log(debug, "OutputUHD:Actual RX Rate: %f sps.", m_usrp->get_rx_rate());

    if (not m_conf.rx_antenna.empty()) {
        m_usrp->set_rx_antenna(m_conf.rx_antenna);
    }
    etiLog.log(debug, "OutputUHD:Actual RX Antenna: %s",
            m_usrp->get_rx_antenna().c_str());

    if (not m_conf.tx_antenna.empty()) {
        m_usrp->set_tx_antenna(m_conf.tx_antenna);
    }
    etiLog.log(debug, "OutputUHD:Actual TX Antenna: %s",
            m_usrp->get_tx_antenna().c_str());

    m_usrp->set_rx_gain(m_conf.rxgain);
    etiLog.log(debug, "OutputUHD:Actual RX Gain: %f", m_usrp->get_rx_gain());

    const uhd::stream_args_t stream_args("fc32"); //complex floats
    m_rx_stream = m_usrp->get_rx_stream(stream_args);
    m_tx_stream = m_usrp->get_tx_stream(stream_args);

    m_running.store(true);
    m_async_rx_thread = std::thread(&UHD::print_async_thread, this);

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

        etiLog.level(debug) << "OutputUHD: TX freq" <<
            std::fixed << std::setprecision(0) <<
            " Target RF: " << result.target_rf_freq <<
            " Actual RF: " << result.actual_rf_freq <<
            " Target DSP: " << result.target_dsp_freq <<
            " Actual DSP: " << result.actual_dsp_freq;

        uhd::tune_result_t result_rx = m_usrp->set_rx_freq(tr);

        etiLog.level(debug) << "OutputUHD: RX freq" <<
            std::fixed << std::setprecision(0) <<
            " Target RF: " << result_rx.target_rf_freq <<
            " Actual RF: " << result_rx.actual_rf_freq <<
            " Target DSP: " << result_rx.target_dsp_freq <<
            " Actual DSP: " << result_rx.actual_dsp_freq;
    }
    else {
        //set the centre frequency
        etiLog.level(info) << std::fixed << std::setprecision(3) <<
            "OutputUHD:Setting freq to " << frequency << "...";
        m_usrp->set_tx_freq(frequency);

        m_usrp->set_rx_freq(frequency);
    }
}

double UHD::get_tx_freq(void) const
{
    return m_usrp->get_tx_freq();
}

void UHD::set_txgain(double txgain)
{
    m_usrp->set_tx_gain(txgain);
    m_conf.txgain = m_usrp->get_tx_gain();
}

double UHD::get_txgain(void) const
{
    return m_usrp->get_tx_gain();
}

void UHD::set_bandwidth(double bandwidth)
{
    m_usrp->set_tx_bandwidth(bandwidth);
    m_usrp->set_rx_bandwidth(bandwidth);
    m_conf.bandwidth = m_usrp->get_tx_bandwidth();
}

double UHD::get_bandwidth(void) const
{
    return m_usrp->get_tx_bandwidth();
}

void UHD::transmit_frame(const struct FrameData& frame)
{
    const double tx_timeout = 20.0;
    const size_t sizeIn = frame.buf.size() / sizeof(complexf);
    const complexf* in_data = reinterpret_cast<const complexf*>(&frame.buf[0]);

    uhd::tx_metadata_t md_tx;

    bool tx_allowed = true;

    // muting and mutenotimestamp is handled by SDR
    if (m_conf.enableSync and frame.ts.timestamp_valid) {
        uhd::time_spec_t timespec(
                frame.ts.timestamp_sec, frame.ts.pps_offset());
        md_tx.time_spec = timespec;
        md_tx.has_time_spec = true;
    }
    else {
        md_tx.has_time_spec = false;
    }

    size_t usrp_max_num_samps = m_tx_stream->get_max_num_samps();
    size_t num_acc_samps = 0; //number of accumulated samples
    while (tx_allowed and m_running.load() and (num_acc_samps < sizeIn)) {
        size_t samps_to_send = std::min(sizeIn - num_acc_samps, usrp_max_num_samps);

        const bool eob_because_muting = m_conf.muting;

        // ensure the the last packet has EOB set if the timestamps has been
        // refreshed and need to be reconsidered. If muting was set, set the
        // EOB and quit the loop afterwards, to avoid an underrun.
        md_tx.end_of_burst = eob_because_muting or (
                frame.ts.timestamp_valid and
                frame.ts.timestamp_refresh and
                samps_to_send <= usrp_max_num_samps );

        //send a single packet
        size_t num_tx_samps = m_tx_stream->send(
                &in_data[num_acc_samps],
                samps_to_send, md_tx, tx_timeout);
        etiLog.log(trace, "UHD,sent %zu of %zu", num_tx_samps, samps_to_send);

        num_acc_samps += num_tx_samps;

        md_tx.time_spec += uhd::time_spec_t(0, num_tx_samps/m_conf.sampleRate);

        if (num_tx_samps == 0) {
            etiLog.log(warn,
                    "OutputUHD unable to write to device, skipping frame!");
            break;
        }

        if (eob_because_muting) {
            break;
        }
    }

    num_frames_modulated++;
}


SDRDevice::RunStatistics UHD::get_run_statistics(void) const
{
    RunStatistics rs;
    rs.num_underruns = num_underflows;
    rs.num_overruns = num_overflows;
    rs.num_late_packets = num_late_packets;
    rs.num_frames_modulated = num_frames_modulated;

    if (m_device_time) {
        const auto gpsdo_stat = m_device_time->get_gnss_stats();
        rs.gpsdo_holdover = gpsdo_stat.holdover;
        rs.gpsdo_num_sv = gpsdo_stat.num_sv;
    }
    return rs;
}

double UHD::get_real_secs(void) const
{
    return m_usrp->get_time_now().get_real_secs();
}

void UHD::set_rxgain(double rxgain)
{
    m_usrp->set_rx_gain(m_conf.rxgain);
    m_conf.rxgain = m_usrp->get_rx_gain();
}

double UHD::get_rxgain() const
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

    uhd::rx_metadata_t md_rx;

    constexpr double timeout = 60;
    size_t samples_read = m_rx_stream->recv(buf, num_samples, md_rx, timeout);

    // Update the ts with the effective receive TS
    ts.timestamp_sec = md_rx.time_spec.get_full_secs();
    ts.timestamp_pps = md_rx.time_spec.get_frac_secs() * 16384000.0;
    return samples_read;
}

// Return true if GPS and reference clock inputs are ok
bool UHD::is_clk_source_ok(void) const
{
    bool ok = true;

    if (refclk_loss_needs_check()) {
        try {
            if (not m_usrp->get_mboard_sensor("ref_locked", 0).to_bool()) {
                ok = false;

                etiLog.level(alert) <<
                    "OutputUHD: External reference clock lock lost !";

                if (m_conf.refclk_lock_loss_behaviour == CRASH) {
                    throw std::runtime_error(
                            "OutputUHD: External reference clock lock lost.");
                }
            }
        }
        catch (const uhd::lookup_error &e) {
            suppress_refclk_loss_check = true;
            etiLog.log(warn, "OutputUHD: This USRP does not have mboard "
                    "sensor for ext clock loss. Check disabled.");
        }
    }

    if (m_device_time) {
        ok &= m_device_time->verify_time();
    }

    return ok;
}

const char* UHD::device_name(void) const
{
    return "UHD";
}

double UHD::get_temperature(void) const
{
    try {
        return std::round(m_usrp->get_tx_sensor("temp", 0).to_real());
    }
    catch (const uhd::lookup_error &e) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

bool UHD::refclk_loss_needs_check() const
{
    if (suppress_refclk_loss_check) {
        return false;
    }
    return m_conf.refclk_src != "internal";
}

void UHD::stop_threads()
{
    m_running.store(false);
    if (m_async_rx_thread.joinable()) {
        m_async_rx_thread.join();
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
                    async_md.time_spec.get_real_secs();
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
                        num_underflows - num_underflows_previous,
                        num_late_packets - num_late_packets_previous);
            }

            num_underflows_previous = num_underflows;
            num_late_packets_previous = num_late_packets;

            last_print_time = time_now;
        }
    }
}

} // namespace Output

#endif // HAVE_OUTPUT_UHD

