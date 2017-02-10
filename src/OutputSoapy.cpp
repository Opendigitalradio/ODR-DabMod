/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "OutputSoapy.h"
#ifdef HAVE_SOAPYSDR

#include <SoapySDR/Errors.hpp>
#include <deque>
#include <chrono>

#include "Log.h"
#include "Utils.h"

#include <stdio.h>

static const size_t FRAMES_MAX_SIZE = 2;


using namespace std;



OutputSoapy::OutputSoapy(OutputSoapyConfig& config) :
    ModOutput(),
    RemoteControllable("soapy"),
    m_conf(config),
    m_device(nullptr)
{
    RC_ADD_PARAMETER(txgain, "SoapySDR analog daughterboard TX gain");
    RC_ADD_PARAMETER(freq,   "SoapySDR transmission frequency");
    RC_ADD_PARAMETER(overflows, "SoapySDR overflow count [r/o]");
    RC_ADD_PARAMETER(underflows, "SoapySDR underflow count [r/o]");

    etiLog.level(info) <<
        "OutputSoapy:Creating the device with: " <<
        config.device;
    try
    {
        m_device = SoapySDR::Device::make(config.device);
        stringstream ss;
        ss << "SoapySDR driver=" << m_device->getDriverKey();
        ss << " hardware=" << m_device->getHardwareKey();
        for (const auto &it : m_device->getHardwareInfo())
        {
            ss << "  " << it.first << "=" << it.second;
        }
    }
    catch (const std::exception &ex)
    {
        etiLog.level(error) << "Error making SoapySDR device: " <<
            ex.what();
        throw std::runtime_error("Cannot create SoapySDR output");
    }

    m_device->setMasterClockRate(config.masterClockRate);
    etiLog.level(info) << "SoapySDR master clock rate set to " <<
        m_device->getMasterClockRate()/1000.0 << " kHz";

    m_device->setSampleRate(SOAPY_SDR_TX, 0, m_conf.sampleRate);
    etiLog.level(info) << "OutputSoapySDR:Actual TX rate: " <<
        m_device->getSampleRate(SOAPY_SDR_TX, 0) / 1000.0 <<
        " ksps.";

    m_device->setFrequency(SOAPY_SDR_TX, 0, m_conf.frequency);
    m_conf.frequency = m_device->getFrequency(SOAPY_SDR_TX, 0);
    etiLog.level(info) << "OutputSoapySDR:Actual frequency: " <<
        m_conf.frequency / 1000.0 <<
        " kHz.";

    m_device->setGain(SOAPY_SDR_TX, 0, m_conf.txgain);
    etiLog.level(info) << "OutputSoapySDR:Actual tx gain: " <<
        m_device->getGain(SOAPY_SDR_TX, 0);

}

OutputSoapy::~OutputSoapy()
{
    m_worker.stop();
    if (m_device != nullptr) {
        SoapySDR::Device::unmake(m_device);
    }
}

void SoapyWorker::stop()
{
    running = false;
    queue.push({});
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void SoapyWorker::start(SoapySDR::Device *device)
{
    m_device = device;
    underflows = 0;
    overflows = 0;
    running = true;
    m_thread = std::thread(&SoapyWorker::process_start, this);
}

void SoapyWorker::process_start()
{
    // Set thread priority to realtime
    if (int ret = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for SoapySDR worker:" << ret;
    }

    set_thread_name("soapyworker");

    std::vector<size_t> channels;
    channels.push_back(0);
    auto stream = m_device->setupStream(SOAPY_SDR_TX, "CF32", channels);
    m_device->activateStream(stream);
    process(stream);
    m_device->closeStream(stream);
    running = false;
    etiLog.level(warn) << "SoapySDR worker terminated";
}

void SoapyWorker::process(SoapySDR::Stream *stream)
{
    while (running) {
        struct SoapyWorkerFrameData frame;
        queue.wait_and_pop(frame);

        // The frame buffer contains bytes representing FC32 samples
        const complexf *buf = reinterpret_cast<complexf*>(frame.buf.data());
        const size_t numSamples = frame.buf.size() / sizeof(complexf);
        if ((frame.buf.size() % sizeof(complexf)) != 0) {
            throw std::runtime_error("OutputSoapy: invalid buffer size");
        }

        // Stream MTU is in samples, not bytes.
        const size_t mtu = m_device->getStreamMTU(stream);

        size_t num_acc_samps = 0;
        while (running && (num_acc_samps < numSamples)) {
            const void *buffs[1];
            buffs[0] = buf + num_acc_samps;

            const size_t samps_to_send = std::min(numSamples - num_acc_samps, mtu);

            int flags = 0;

            auto ret = m_device->writeStream(stream, buffs, samps_to_send, flags);

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
                running = false;
            }

            num_acc_samps += ret;
        }
    }
}

int OutputSoapy::process(Buffer* dataIn)
{
    if (first_run) {
        m_worker.start(m_device);
        first_run = false;
    }
    else if (!m_worker.running) {
        etiLog.level(error) << "OutputSoapy: worker thread died";
        throw std::runtime_error("Fault in OutputSoapy");
    }

    SoapyWorkerFrameData frame;
    m_eti_source->calculateTimestamp(frame.ts);


    if (frame.ts.fct == -1) {
        etiLog.level(info) <<
            "OutputSoapy: dropping one frame with invalid FCT";
    }
    else {
        const uint8_t* pInData = reinterpret_cast<uint8_t*>(dataIn->getData());
        frame.buf.resize(dataIn->getLength());
        std::copy(pInData, pInData + dataIn->getLength(),
                frame.buf.begin());
        m_worker.queue.push_wait_if_full(frame, FRAMES_MAX_SIZE);
    }

    return dataIn->getLength();
}


void OutputSoapy::setETISource(EtiSource *etiSource)
{
    m_eti_source = etiSource;
}

void OutputSoapy::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "txgain") {
        ss >> m_conf.txgain;
        m_device->setGain(SOAPY_SDR_TX, 0, m_conf.txgain);
    }
    else if (parameter == "freq") {
        ss >> m_conf.frequency;
        m_device->setFrequency(SOAPY_SDR_TX, 0, m_conf.frequency);
    m_conf.frequency = m_device->getFrequency(SOAPY_SDR_TX, 0);
    }
    else if (parameter == "underflows") {
        throw ParameterError("Parameter 'underflows' is read-only");
    }
    else if (parameter == "overflows") {
        throw ParameterError("Parameter 'overflows' is read-only");
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string OutputSoapy::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "txgain") {
        ss << m_conf.txgain;
    }
    else if (parameter == "freq") {
        ss << m_conf.frequency;
    }
    else if (parameter == "underflows") {
        ss << m_worker.underflows;
    }
    else if (parameter == "overflows") {
        ss << m_worker.overflows;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

#endif // HAVE_SOAPYSDR

