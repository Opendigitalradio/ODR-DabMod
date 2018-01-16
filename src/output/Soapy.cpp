/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver using the SoapySDR library that can output to
   many devices.
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

#include "output/Soapy.h"

#ifdef HAVE_SOAPYSDR

#include <SoapySDR/Errors.hpp>
#include <chrono>
#include <cstdio>

#include "Log.h"
#include "Utils.h"

using namespace std;

namespace Output {

static constexpr size_t FRAMES_MAX_SIZE = 2;

Soapy::Soapy(SDRDeviceConfig& config) :
    SDRDevice(),
    m_conf(config)
{
    etiLog.level(info) <<
        "Soapy:Creating the device with: " <<
        m_conf.device;

    try {
        m_device = SoapySDR::Device::make(m_conf.device);
        stringstream ss;
        ss << "SoapySDR driver=" << m_device->getDriverKey();
        ss << " hardware=" << m_device->getHardwareKey();
        for (const auto &it : m_device->getHardwareInfo()) {
            ss << "  " << it.first << "=" << it.second;
        }
    }
    catch (const std::exception &ex) {
        etiLog.level(error) << "Error making SoapySDR device: " <<
            ex.what();
        throw std::runtime_error("Cannot create SoapySDR output");
    }

    m_device->setMasterClockRate(m_conf.masterClockRate);
    etiLog.level(info) << "SoapySDR master clock rate set to " <<
        m_device->getMasterClockRate()/1000.0 << " kHz";

    m_device->setSampleRate(SOAPY_SDR_TX, 0, m_conf.sampleRate);
    etiLog.level(info) << "SoapySDR:Actual TX rate: " <<
        m_device->getSampleRate(SOAPY_SDR_TX, 0) / 1000.0 <<
        " ksps.";

    tune(m_conf.lo_offset, m_conf.frequency);
    m_conf.frequency = m_device->getFrequency(SOAPY_SDR_TX, 0);
    etiLog.level(info) << "SoapySDR:Actual frequency: " <<
        m_conf.frequency / 1000.0 <<
        " kHz.";

    m_device->setGain(SOAPY_SDR_TX, 0, m_conf.txgain);
    etiLog.level(info) << "SoapySDR:Actual tx gain: " <<
        m_device->getGain(SOAPY_SDR_TX, 0);

    const std::vector<size_t> channels({0});
    m_tx_stream = m_device->setupStream(SOAPY_SDR_TX, "CF32", channels);
    m_device->activateStream(m_tx_stream);

    m_rx_stream = m_device->setupStream(SOAPY_SDR_RX, "CF32", channels);
}

Soapy::~Soapy()
{
    if (m_device != nullptr) {
        if (m_tx_stream != nullptr) {
            m_device->closeStream(m_tx_stream);
        }
        SoapySDR::Device::unmake(m_device);
    }
}

void Soapy::tune(double lo_offset, double frequency)
{
    if (not m_device) throw runtime_error("Soapy device not set up");

    SoapySDR::Kwargs offset_arg;
    offset_arg["OFFSET"] = to_string(lo_offset);
    m_device->setFrequency(SOAPY_SDR_TX, 0, m_conf.frequency, offset_arg);
}

double Soapy::get_tx_freq(void) const
{
    if (not m_device) throw runtime_error("Soapy device not set up");

    // TODO lo offset
    return m_device->getFrequency(SOAPY_SDR_TX, 0);
}

void Soapy::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device) throw runtime_error("Soapy device not set up");
    m_device->setGain(SOAPY_SDR_TX, 0, m_conf.txgain);
}

double Soapy::get_txgain(void) const
{
    if (not m_device) throw runtime_error("Soapy device not set up");
    return m_device->getGain(SOAPY_SDR_TX, 0);
}

SDRDevice::RunStatistics Soapy::get_run_statistics(void) const
{
    RunStatistics rs;
    rs.num_underruns = underflows;
    rs.num_overruns = overflows;
    rs.num_late_packets = late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}


double Soapy::get_real_secs(void) const
{
    if (m_device) {
        long long time_ns = m_device->getHardwareTime();
        return time_ns / 1e9;
    }
    else {
        return 0.0;
    }
}

void Soapy::set_rxgain(double rxgain)
{
    m_device->setGain(SOAPY_SDR_RX, 0, m_conf.rxgain);
    m_conf.rxgain = m_device->getGain(SOAPY_SDR_RX, 0);
}

double Soapy::get_rxgain(void) const
{
    return m_device->getGain(SOAPY_SDR_RX, 0);
}

size_t Soapy::receive_frame(
        complexf *buf,
        size_t num_samples,
        struct frame_timestamp& ts,
        double timeout_secs)
{
    int flags = 0;
    long long timeNs = ts.get_ns();
    const size_t numElems = num_samples;

    void *buffs[1];
    buffs[0] = buf;

    m_device->activateStream(m_rx_stream, flags, timeNs, numElems);

    auto ret = m_device->readStream(m_tx_stream, buffs, num_samples, flags, timeNs);

    m_device->deactivateStream(m_rx_stream);

    // TODO update effective receive ts

    if (ret < 0) {
        throw runtime_error("Soapy readStream error: " + to_string(ret));
    }

    return ret;
}


bool Soapy::is_clk_source_ok() const
{
    // TODO
    return true;
}

const char* Soapy::device_name(void) const
{
    return "Soapy";
}

void Soapy::transmit_frame(const struct FrameData& frame)
{
    if (not m_device) throw runtime_error("Soapy device not set up");

    // TODO timestamps

    // The frame buffer contains bytes representing FC32 samples
    const complexf *buf = reinterpret_cast<const complexf*>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);
    if ((frame.buf.size() % sizeof(complexf)) != 0) {
        throw std::runtime_error("Soapy: invalid buffer size");
    }

    // Stream MTU is in samples, not bytes.
    const size_t mtu = m_device->getStreamMTU(m_tx_stream);

    size_t num_acc_samps = 0;
    while (num_acc_samps < numSamples) {
        const void *buffs[1];
        buffs[0] = buf + num_acc_samps;

        const size_t samps_to_send = std::min(numSamples - num_acc_samps, mtu);

        int flags = 0;

        auto ret = m_device->writeStream(m_tx_stream, buffs, samps_to_send, flags);

        if (ret == SOAPY_SDR_TIMEOUT) {
            continue;
        }
        else if (ret == SOAPY_SDR_OVERFLOW) {
            overflows++;
            continue;
        }
        else if (ret == SOAPY_SDR_UNDERFLOW) {
            underflows++;
            continue;
        }

        if (ret < 0) {
            etiLog.level(error) << "Unexpected stream error " <<
                SoapySDR::errToStr(ret);
            throw std::runtime_error("Fault in Soapy");
        }

        num_acc_samps += ret;
    }
}

} // namespace Output

#endif // HAVE_SOAPYSDR


