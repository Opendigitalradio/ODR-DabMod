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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#ifdef HAVE_SOAPYSDR
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Device.hpp>

#include <string>
#include <memory>

#include "ModPlugin.h"
#include "EtiReader.h"
#include "RemoteControl.h"
#include "ThreadsafeQueue.h"

typedef std::complex<float> complexf;

/* This structure is used as initial configuration for the Soapy output.
 * It must also contain all remote-controllable settings, otherwise
 * they will get lost on a modulator restart. */
struct OutputSoapyConfig {
    std::string device;

    long masterClockRate = 32768000;
    unsigned sampleRate = 2048000;
    double frequency = 0.0;
    double txgain = 0.0;
    unsigned dabMode = 0;
};

// Each frame contains one OFDM frame, and its
// associated timestamp
struct SoapyWorkerFrameData {
    // Buffer holding frame data
    std::vector<uint8_t> buf;

    // A full timestamp contains a TIST according to standard
    // and time information within MNSC with tx_second.
    struct frame_timestamp ts;
};

class SoapyWorker
{
    public:
        ThreadsafeQueue<SoapyWorkerFrameData> queue;
        SoapySDR::Device *m_device;
        std::atomic<bool> running;
        size_t underflows;
        size_t overflows;

        SoapyWorker() {}
        SoapyWorker(const SoapyWorker&) = delete;
        SoapyWorker operator=(const SoapyWorker&) = delete;
        ~SoapyWorker() { stop(); }

        void start(SoapySDR::Device *device);
        void stop(void);

    private:
        std::thread m_thread;

        void process_start(void);
        void process(SoapySDR::Stream *stream);
};

class OutputSoapy: public ModOutput, public RemoteControllable
{
    public:
        OutputSoapy(OutputSoapyConfig& config);
        OutputSoapy(const OutputSoapy& other) = delete;
        OutputSoapy& operator=(const OutputSoapy& other) = delete;
        ~OutputSoapy();

        int process(Buffer* dataIn);

        const char* name() { return "OutputSoapy"; }

        void setETISource(EtiSource *etiSource);

        /*********** REMOTE CONTROL ***************/

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const;


    protected:
        SoapyWorker m_worker;
        EtiSource *m_eti_source;
        OutputSoapyConfig& m_conf;

        SoapySDR::Device *m_device;

        bool first_run = true;
};


#endif //HAVE_SOAPYSDR
