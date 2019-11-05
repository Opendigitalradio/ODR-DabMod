/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#include "output/SDR.h"
#include "output/Lime.h"

#include "PcDebug.h"
#include "Log.h"
#include "RemoteControl.h"
#include "Utils.h"

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
static constexpr size_t FRAMES_MAX_SIZE = 8;

// If the timestamp is further in the future than
// 100 seconds, abort
static constexpr double TIMESTAMP_ABORT_FUTURE = 100;

// Add a delay to increase buffers when
// frames are too far in the future
static constexpr double TIMESTAMP_MARGIN_FUTURE = 0.5;

SDR::SDR(SDRDeviceConfig& config, std::shared_ptr<SDRDevice> device) :
    ModOutput(), ModMetadata(), RemoteControllable("sdr"),
    m_config(config),
    m_device(device)
{
    // muting is remote-controllable
    m_config.muting = false;

    m_device_thread = std::thread(&SDR::process_thread_entry, this);

    if (m_config.dpdFeedbackServerPort > 0) {
        m_dpd_feedback_server = make_shared<DPDFeedbackServer>(
                m_device,
                m_config.dpdFeedbackServerPort,
                m_config.sampleRate);
    }

    RC_ADD_PARAMETER(txgain, "TX gain");
    RC_ADD_PARAMETER(rxgain, "RX gain for DPD feedback");
    RC_ADD_PARAMETER(bandwidth, "Analog front-end bandwidth");
    RC_ADD_PARAMETER(freq, "Transmission frequency");
    RC_ADD_PARAMETER(muting, "Mute the output by stopping the transmitter");
    RC_ADD_PARAMETER(temp, "Temperature in degrees C of the device");
    RC_ADD_PARAMETER(underruns, "Counter of number of underruns");
    RC_ADD_PARAMETER(latepackets, "Counter of number of late packets");
    RC_ADD_PARAMETER(frames, "Counter of number of frames modulated");
    RC_ADD_PARAMETER(gpsdo_num_sv, "Number of Satellite Vehicles tracked by GPSDO");
    RC_ADD_PARAMETER(gpsdo_holdover, "1 if the GPSDO is in holdover, 0 if it is using gnss");

#ifdef HAVE_LIMESDR
    if (std::dynamic_pointer_cast<Lime>(device)) {
        RC_ADD_PARAMETER(fifo_fill, "A value representing the Lime FIFO fullness [percent]");
    }
#endif // HAVE_LIMESDR
}

SDR::~SDR()
{
    m_running.store(false);

    m_queue.trigger_wakeup();

    if (m_device_thread.joinable()) {
        m_device_thread.join();
    }
}

int SDR::process(Buffer *dataIn)
{
    if (not m_running) {
        throw std::runtime_error("SDR thread failed");
    }

    const uint8_t* pDataIn = (uint8_t*)dataIn->getData();
    m_frame.resize(dataIn->getLength());
    std::copy(pDataIn, pDataIn + dataIn->getLength(),
            m_frame.begin());

    // We will effectively transmit the frame once we got the metadata.

    return dataIn->getLength();
}

meta_vec_t SDR::process_metadata(const meta_vec_t& metadataIn)
{
    if (m_device and m_running) {
        FrameData frame;
        frame.buf = std::move(m_frame);

        if (metadataIn.empty()) {
            etiLog.level(info) <<
                "SDR output: dropping one frame with invalid FCT";
        }
        else {
            /* In transmission modes where several ETI frames are needed to
             * build one transmission frame (like in TM 1), we will have
             * several entries in metadataIn. Take the first one, which
             * comes from the earliest ETI frame.
             * This behaviour is different to earlier versions of ODR-DabMod,
             * which took the timestamp from the latest ETI frame.
             */
            frame.ts = *(metadataIn[0].ts);

            // TODO check device running

            try {
                if (m_dpd_feedback_server) {
                    m_dpd_feedback_server->set_tx_frame(frame.buf, frame.ts);
                }
            }
            catch (const runtime_error& e) {
                etiLog.level(warn) <<
                    "SDR output: Feedback server failed, restarting...";

                m_dpd_feedback_server = std::make_shared<DPDFeedbackServer>(
                        m_device,
                        m_config.dpdFeedbackServerPort,
                        m_config.sampleRate);
            }

            size_t num_frames = m_queue.push_wait_if_full(frame,
                    FRAMES_MAX_SIZE);
            etiLog.log(trace, "SDR,push %zu", num_frames);
        }
    }
    else {
        // Ignore frame
    }
    return {};
}


void SDR::process_thread_entry()
{
    // Set thread priority to realtime
    if (int ret = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for SDR device thread:" << ret;
    }

    set_thread_name("sdrdevice");

    last_tx_time_initialised = false;

    size_t last_num_underflows = 0;
    size_t pop_prebuffering = FRAMES_MAX_SIZE;

    m_running.store(true);

    try {
        while (m_running.load()) {
            struct FrameData frame;
            etiLog.log(trace, "SDR,wait");
            m_queue.wait_and_pop(frame, pop_prebuffering);
            etiLog.log(trace, "SDR,pop");

            if (m_running.load() == false) {
                break;
            }

            if (m_device) {
                handle_frame(frame);

                const auto rs = m_device->get_run_statistics();

                /* Ensure we fill frames after every underrun and
                 * at startup to reduce underrun likelihood. */
                if (last_num_underflows < rs.num_underruns) {
                    pop_prebuffering = FRAMES_MAX_SIZE;
                }
                else {
                    pop_prebuffering = 1;
                }

                last_num_underflows = rs.num_underruns;
            }
        }
    }
    catch (const ThreadsafeQueueWakeup& e) { }
    catch (const runtime_error& e) {
        etiLog.level(error) << "SDR output thread caught runtime error: " <<
            e.what();
    }

    m_running.store(false);
}

const char* SDR::name()
{
    if (m_device) {
        m_name = "OutputSDR(";
        m_name += m_device->device_name();
        m_name += ")";
    }
    else {
        m_name = "OutputSDR(<no device>)";
    }
    return m_name.c_str();
}

void SDR::sleep_through_frame()
{
    using namespace std::chrono;

    const auto now = steady_clock::now();

    if (not t_last_frame_initialised) {
        t_last_frame = now;
        t_last_frame_initialised = true;
    }

    const auto delta = now - t_last_frame;
    const auto wait_time = transmission_frame_duration(m_config.dabMode);

    if (wait_time > delta) {
        this_thread::sleep_for(wait_time - delta);
    }

    t_last_frame += wait_time;
}

void SDR::handle_frame(struct FrameData& frame)
{
    // Assumes m_device is valid

    constexpr double tx_timeout = 20.0;

    if (not m_device->is_clk_source_ok()) {
        sleep_through_frame();
        return;
    }

    const auto& time_spec = frame.ts;

    if (m_config.enableSync and m_config.muteNoTimestamps and
            not time_spec.timestamp_valid) {
        sleep_through_frame();
        etiLog.log(info,
                "OutputSDR: Muting sample %d : no timestamp\n",
                frame.ts.fct);
        return;
    }

    if (m_config.enableSync and time_spec.timestamp_valid) {
        // Tx time from MNSC and TIST
        const uint32_t tx_second = frame.ts.timestamp_sec;
        const uint32_t tx_pps    = frame.ts.timestamp_pps;

        const double device_time = m_device->get_real_secs();

        if (not frame.ts.timestamp_valid) {
            /* We have not received a full timestamp through
             * MNSC. We sleep through the frame.
             */
            etiLog.level(info) <<
                "OutputSDR: Throwing sample " << frame.ts.fct <<
                " away: incomplete timestamp " << tx_second <<
                " / " << tx_pps;
            return;
        }

        if (last_tx_time_initialised) {
            const size_t sizeIn = frame.buf.size() / sizeof(complexf);

            // Checking units for the increment calculation:
            // samps  * ticks/s  / (samps/s)
            // (samps * ticks * s) / (s * samps)
            // ticks
            const uint64_t increment = (uint64_t)sizeIn * 16384000ul /
                                       (uint64_t)m_config.sampleRate;

            uint32_t expected_sec = last_tx_second + increment / 16384000ul;
            uint32_t expected_pps = last_tx_pps + increment % 16384000ul;

            while (expected_pps >= 16384000) {
                expected_sec++;
                expected_pps -= 16384000;
            }

            if (expected_sec != tx_second or expected_pps != tx_pps) {
                etiLog.level(warn) << "OutputSDR: timestamp irregularity at FCT=" << frame.ts.fct <<
                    std::fixed <<
                    " Expected " <<
                    expected_sec << "+" << (double)expected_pps/16384000.0 <<
                    "(" << expected_pps << ")" <<
                    " Got " <<
                    tx_second << "+" << (double)tx_pps/16384000.0 <<
                    "(" << tx_pps << ")";

                frame.ts.timestamp_refresh = true;
            }
        }

        last_tx_second = tx_second;
        last_tx_pps    = tx_pps;
        last_tx_time_initialised = true;

        const double pps_offset = tx_pps / 16384000.0;

        etiLog.log(trace, "SDR,tist %f", time_spec.get_real_secs());

        if (time_spec.get_real_secs() + tx_timeout < device_time) {
            etiLog.level(warn) <<
                "OutputSDR: Timestamp in the past at FCT=" << frame.ts.fct << " offset: " <<
                std::fixed <<
                time_spec.get_real_secs() - device_time <<
                "  (" << device_time << ")"
                " frame " << frame.ts.fct <<
                ", tx_second " << tx_second <<
                ", pps " << pps_offset;
            return;
        }

        if (time_spec.get_real_secs() > device_time + TIMESTAMP_ABORT_FUTURE) {
            etiLog.level(error) <<
                "OutputSDR: Timestamp way too far in the future at FCT=" << frame.ts.fct << " offset: " <<
                std::fixed <<
                time_spec.get_real_secs() - device_time;
            throw std::runtime_error("Timestamp error. Aborted.");
        }
    }

    if (m_config.muting) {
        etiLog.log(info,
                "OutputSDR: Muting FCT=%d requested",
                frame.ts.fct);
        return;
    }

    m_device->transmit_frame(frame);
}

// =======================================
// Remote Control
// =======================================
void SDR::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "txgain") {
        ss >> m_config.txgain;
        m_device->set_txgain(m_config.txgain);
    }
    else if (parameter == "rxgain") {
        ss >> m_config.rxgain;
        m_device->set_rxgain(m_config.rxgain);
    }
    else if (parameter == "bandwidth") {
        ss >> m_config.bandwidth;
        m_device->set_bandwidth(m_config.bandwidth);
    }
    else if (parameter == "freq") {
        ss >> m_config.frequency;
        m_device->tune(m_config.lo_offset, m_config.frequency);
        m_config.frequency = m_device->get_tx_freq();
    }
    else if (parameter == "muting") {
        ss >> m_config.muting;
    }
    else if (parameter == "underruns" or
             parameter == "latepackets" or
             parameter == "frames" or
             parameter == "gpsdo_num_sv" or
             parameter == "gpsdo_holdover" or
             parameter == "fifo_fill") {
        throw ParameterError("Parameter " + parameter + " is read-only.");
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
    }
}

const string SDR::get_parameter(const string& parameter) const
{
    stringstream ss;
    ss << std::fixed;
    if (parameter == "txgain") {
        ss << m_config.txgain;
    }
    else if (parameter == "rxgain") {
        ss << m_config.rxgain;
    }
    else if (parameter == "bandwidth") {
        ss << m_config.bandwidth;
    }
    else if (parameter == "freq") {
        ss << m_config.frequency;
    }
    else if (parameter == "muting") {
        ss << m_config.muting;
    }
    else if (parameter == "temp") {
        if (not m_device) {
            throw ParameterError("OutputSDR has no device");
        }
        const double temp = m_device->get_temperature();
        if (std::isnan(temp)) {
            throw ParameterError("Temperature not available");
        }
        else {
            ss << temp;
        }
    }
    else if (parameter == "underruns" or
            parameter == "latepackets" or
            parameter == "frames" ) {
        if (not m_device) {
            throw ParameterError("OutputSDR has no device");
        }
        const auto stat = m_device->get_run_statistics();

        if (parameter == "underruns") {
            ss << stat.num_underruns;
        }
        else if (parameter == "latepackets") {
            ss << stat.num_late_packets;
        }
        else if (parameter == "frames") {
            ss << stat.num_frames_modulated;
        }
    }
    else if (parameter == "gpsdo_num_sv") {
        const auto stat = m_device->get_run_statistics();
        ss << stat.gpsdo_num_sv;
    }
    else if (parameter == "gpsdo_holdover") {
        const auto stat = m_device->get_run_statistics();
        ss << (stat.gpsdo_holdover ? 1 : 0);
    }
#ifdef HAVE_LIMESDR
    else if (parameter == "fifo_fill") {
        const auto dev = std::dynamic_pointer_cast<Lime>(m_device);

        if (dev) {
            ss << dev->get_fifo_fill_percent();
        }
        else {
            ss << "Parameter '" << parameter <<
                "' is not exported by controllable " << get_rc_name();
            throw ParameterError(ss.str());
        }
    }
#endif // HAVE_LIMESDR
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

} // namespace Output
