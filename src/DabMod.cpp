/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "porting.h"
#include "Utils.h"
#include "Log.h"
#include "DabModulator.h"
#include "InputMemory.h"
#include "OutputFile.h"
#include "FormatConverter.h"
#include "FrameMultiplexer.h"
#include "output/SDR.h"
#include "output/UHD.h"
#include "output/Soapy.h"
#include "OutputZeroMQ.h"
#include "InputReader.h"
#include "PcDebug.h"
#include "TimestampDecoder.h"
#include "FIRFilter.h"
#include "RemoteControl.h"
#include "ConfigParser.h"

#include <memory>
#include <complex>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>
#include <signal.h>

#if HAVE_NETINET_IN_H
#   include <netinet/in.h>
#endif

/* UHD requires the input I and Q samples to be in the interval
 * [-1.0,1.0], otherwise they get truncated, which creates very
 * wide-spectrum spikes. Depending on the Transmission Mode, the
 * Gain Mode and the sample rate (and maybe other parameters), the
 * samples can have peaks up to about 48000. The value of 50000
 * should guarantee that with a digital gain of 1.0, UHD never clips
 * our samples.
 */
static const float normalise_factor = 50000.0f;

//Empirical normalisation factors used to normalise the samples to amplitude 1.
static const float normalise_factor_file_fix = 81000.0f;
static const float normalise_factor_file_var = 46000.0f;
static const float normalise_factor_file_max = 46000.0f;

typedef std::complex<float> complexf;

using namespace std;

volatile sig_atomic_t running = 1;

void signalHandler(int signalNb)
{
    PDEBUG("signalHandler(%i)\n", signalNb);

    running = 0;
}

struct modulator_data
{
    modulator_data() :
        inputReader(nullptr),
        framecount(0),
        flowgraph(nullptr),
        etiReader(nullptr) {}

    InputReader* inputReader;
    Buffer data;
    uint64_t framecount;

    Flowgraph* flowgraph;
    EtiReader* etiReader;
};

enum class run_modulator_state_t {
    failure,    // Corresponds to all failures
    normal_end, // Number of frames to modulate was reached
    again,      // ZeroMQ overrun
    reconfigure // Some sort of change of configuration we cannot handle happened
};

run_modulator_state_t run_modulator(modulator_data& m);

static void printModSettings(const mod_settings_t& mod_settings)
{
    // Print settings
    fprintf(stderr, "Input\n");
    fprintf(stderr, "  Type: %s\n", mod_settings.inputTransport.c_str());
    fprintf(stderr, "  Source: %s\n", mod_settings.inputName.c_str());
    fprintf(stderr, "Output\n");

    if (mod_settings.useFileOutput) {
        fprintf(stderr, "  Name: %s\n", mod_settings.outputName.c_str());
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (mod_settings.useUHDOutput) {
        fprintf(stderr, " UHD\n"
                        "  Device: %s\n"
                        "  Subdevice: %s\n"
                        "  master_clock_rate: %ld\n"
                        "  refclk: %s\n"
                        "  pps source: %s\n",
                mod_settings.sdr_device_config.device.c_str(),
                mod_settings.sdr_device_config.subDevice.c_str(),
                mod_settings.sdr_device_config.masterClockRate,
                mod_settings.sdr_device_config.refclk_src.c_str(),
                mod_settings.sdr_device_config.pps_src.c_str());
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (mod_settings.useSoapyOutput) {
        fprintf(stderr, " SoapySDR\n"
                        "  Device: %s\n"
                        "  master_clock_rate: %ld\n",
                mod_settings.sdr_device_config.device.c_str(),
                mod_settings.sdr_device_config.masterClockRate);
    }
#endif
    else if (mod_settings.useZeroMQOutput) {
        fprintf(stderr, " ZeroMQ\n"
                        "  Listening on: %s\n"
                        "  Socket type : %s\n",
                        mod_settings.outputName.c_str(),
                        mod_settings.zmqOutputSocketType.c_str());
    }

    fprintf(stderr, "  Sampling rate: ");
    if (mod_settings.outputRate > 1000) {
        if (mod_settings.outputRate > 1000000) {
            fprintf(stderr, "%.4g MHz\n", mod_settings.outputRate / 1000000.0);
        } else {
            fprintf(stderr, "%.4g kHz\n", mod_settings.outputRate / 1000.0);
        }
    } else {
        fprintf(stderr, "%zu Hz\n", mod_settings.outputRate);
    }
}

static shared_ptr<ModOutput> prepare_output(
        mod_settings_t& s)
{
    shared_ptr<ModOutput> output;

    if (s.useFileOutput) {
        if (s.fileOutputFormat == "complexf") {
            output = make_shared<OutputFile>(s.outputName);
        }
        else if (s.fileOutputFormat == "complexf_normalised") {
            if (s.gainMode == GainMode::GAIN_FIX)
                s.normalise = 1.0f / normalise_factor_file_fix;
            else if (s.gainMode == GainMode::GAIN_MAX)
                s.normalise = 1.0f / normalise_factor_file_max;
            else if (s.gainMode == GainMode::GAIN_VAR)
                s.normalise = 1.0f / normalise_factor_file_var;
            output = make_shared<OutputFile>(s.outputName);
        }
        else if (s.fileOutputFormat == "s8" or
                s.fileOutputFormat == "u8") {
            // We must normalise the samples to the interval [-127.0; 127.0]
            // The formatconverter will add 127 for u8 so that it ends up in
            // [0; 255]
            s.normalise = 127.0f / normalise_factor;

            output = make_shared<OutputFile>(s.outputName);
        }
        else {
            throw runtime_error("File output format " + s.fileOutputFormat +
                    " not known");
        }
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (s.useUHDOutput) {
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto uhddevice = make_shared<Output::UHD>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, uhddevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (s.useSoapyOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto soapydevice = make_shared<Output::Soapy>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, soapydevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_ZEROMQ)
    else if (s.useZeroMQOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        if (s.zmqOutputSocketType == "pub") {
            output = make_shared<OutputZeroMQ>(s.outputName, ZMQ_PUB);
        }
        else if (s.zmqOutputSocketType == "rep") {
            output = make_shared<OutputZeroMQ>(s.outputName, ZMQ_REP);
        }
        else {
            std::stringstream ss;
            ss << "ZeroMQ output socket type " << s.zmqOutputSocketType << " invalid";
            throw std::invalid_argument(ss.str());
        }
    }
#endif

    return output;
}

int launch_modulator(int argc, char* argv[])
{
    int ret = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &signalHandler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    mod_settings_t mod_settings;
    parse_args(argc, argv, mod_settings);

    printStartupInfo();

    if (not (mod_settings.useFileOutput or
             mod_settings.useUHDOutput or
             mod_settings.useZeroMQOutput or
             mod_settings.useSoapyOutput)) {
        etiLog.level(error) << "Output not specified";
        fprintf(stderr, "Must specify output !");
        throw std::runtime_error("Configuration error");
    }

    // When using the FIRFilter, increase the modulator offset pipelining delay
    // by the correct amount
    if (not mod_settings.filterTapsFilename.empty()) {
        mod_settings.tist_delay_stages += FIRFILTER_PIPELINE_DELAY;
    }

    printModSettings(mod_settings);

    modulator_data m;

    shared_ptr<FormatConverter> format_converter;
    if (mod_settings.useFileOutput and
            (mod_settings.fileOutputFormat == "s8" or
             mod_settings.fileOutputFormat == "u8")) {
        format_converter = make_shared<FormatConverter>(mod_settings.fileOutputFormat);
    }

    auto output = prepare_output(mod_settings);

    // Set thread priority to realtime
    if (int r = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for modulator:" << r;
    }
    set_thread_name("modulator");

    if (mod_settings.inputTransport == "edi") {
        EdiReader ediReader(mod_settings.tist_offset_s, mod_settings.tist_delay_stages);
        EdiDecoder::ETIDecoder ediInput(ediReader, false);
        if (mod_settings.edi_max_delay_ms > 0.0f) {
            // setMaxDelay wants number of AF packets, which correspond to 24ms ETI frames
            ediInput.setMaxDelay(lroundf(mod_settings.edi_max_delay_ms / 24.0f));
        }
        EdiUdpInput ediUdpInput(ediInput);

        ediUdpInput.Open(mod_settings.inputName);
        if (not ediUdpInput.isEnabled()) {
            etiLog.level(error) << "inputTransport is edi, but ediUdpInput is not enabled";
            return -1;
        }
        Flowgraph flowgraph;

        auto modulator = make_shared<DabModulator>(ediReader, mod_settings);

        if (format_converter) {
            flowgraph.connect(modulator, format_converter);
            flowgraph.connect(format_converter, output);
        }
        else {
            flowgraph.connect(modulator, output);
        }

        if (false
#if defined(HAVE_OUTPUT_UHD)
                or mod_settings.useUHDOutput
#endif
#if defined(HAVE_SOAPYSDR)
                or mod_settings.useSoapyOutput
#endif
           ) {
            ((Output::SDR*)output.get())->setETISource(modulator->getEtiSource());
        }

        size_t framecount = 0;

        while (running) {
            while (not ediReader.isFrameReady()) {
                bool success = ediUdpInput.rxPacket();
                if (not success) {
                    running = false;
                    break;
                }
            }
            framecount++;
            flowgraph.run();
            ediReader.clearFrame();

            /* Check every once in a while if the remote control
             * is still working */
            if ((framecount % 250) == 0) {
                rcs.check_faults();
            }
        }
    }
    else {
        shared_ptr<InputReader> inputReader;

        if (mod_settings.inputTransport == "file") {
            auto inputFileReader = make_shared<InputFileReader>();

            // Opening ETI input file
            if (inputFileReader->Open(mod_settings.inputName, mod_settings.loop) == -1) {
                fprintf(stderr, "Unable to open input file!\n");
                etiLog.level(error) << "Unable to open input file!";
                ret = -1;
                throw std::runtime_error("Unable to open input");
            }

            inputReader = inputFileReader;
        }
        else if (mod_settings.inputTransport == "zeromq") {
#if !defined(HAVE_ZEROMQ)
            fprintf(stderr, "Error, ZeroMQ input transport selected, but not compiled in!\n");
            ret = -1;
            throw std::runtime_error("Unable to open input");
#else
            auto inputZeroMQReader = make_shared<InputZeroMQReader>();
            inputZeroMQReader->Open(mod_settings.inputName, mod_settings.inputMaxFramesQueued);
            inputReader = inputZeroMQReader;
#endif
        }
        else if (mod_settings.inputTransport == "tcp") {
            auto inputTcpReader = make_shared<InputTcpReader>();
            inputTcpReader->Open(mod_settings.inputName);
            inputReader = inputTcpReader;
        }
        else
        {
            fprintf(stderr, "Error, invalid input transport %s selected!\n", mod_settings.inputTransport.c_str());
            ret = -1;
            throw std::runtime_error("Unable to open input");
        }

        bool run_again = true;

        while (run_again) {
            Flowgraph flowgraph;

            m.inputReader = inputReader.get();
            m.flowgraph = &flowgraph;
            m.data.setLength(6144);

            EtiReader etiReader(mod_settings.tist_offset_s, mod_settings.tist_delay_stages);
            m.etiReader = &etiReader;

            auto input = make_shared<InputMemory>(&m.data);
            auto modulator = make_shared<DabModulator>(etiReader, mod_settings);

            if (format_converter) {
                flowgraph.connect(modulator, format_converter);
                flowgraph.connect(format_converter, output);
            }
            else {
                flowgraph.connect(modulator, output);
            }

            if (false
#if defined(HAVE_OUTPUT_UHD)
                    or mod_settings.useUHDOutput
#endif
#if defined(HAVE_SOAPYSDR)
                    or mod_settings.useSoapyOutput
#endif
               ) {
                ((Output::SDR*)output.get())->setETISource(modulator->getEtiSource());
            }

            inputReader->PrintInfo();

            run_modulator_state_t st = run_modulator(m);
            etiLog.log(trace, "DABMOD,run_modulator() = %d", st);

            switch (st) {
                case run_modulator_state_t::failure:
                    etiLog.level(error) << "Modulator failure.";
                    run_again = false;
                    ret = 1;
                    break;
                case run_modulator_state_t::again:
                    etiLog.level(warn) << "Restart modulator.";
                    run_again = false;
                    if (auto in = dynamic_pointer_cast<InputFileReader>(inputReader)) {
                        if (in->Open(mod_settings.inputName, mod_settings.loop) == -1) {
                            etiLog.level(error) << "Unable to open input file!";
                            ret = 1;
                        }
                        else {
                            run_again = true;
                        }
                    }
#if defined(HAVE_ZEROMQ)
                    else if (dynamic_pointer_cast<InputZeroMQReader>(inputReader)) {
                        run_again = true;
                        // Create a new input reader
                        auto inputZeroMQReader = make_shared<InputZeroMQReader>();
                        inputZeroMQReader->Open(mod_settings.inputName, mod_settings.inputMaxFramesQueued);
                        inputReader = inputZeroMQReader;
                    }
#endif
                    else if (dynamic_pointer_cast<InputTcpReader>(inputReader)) {
                        // Create a new input reader
                        auto inputTcpReader = make_shared<InputTcpReader>();
                        inputTcpReader->Open(mod_settings.inputName);
                        inputReader = inputTcpReader;
                    }
                    break;
                case run_modulator_state_t::reconfigure:
                    etiLog.level(warn) << "Detected change in ensemble configuration.";
                    /* We can keep the input in this care */
                    run_again = true;
                    break;
                case run_modulator_state_t::normal_end:
                default:
                    etiLog.level(info) << "modulator stopped.";
                    ret = 0;
                    run_again = false;
                    break;
            }

            fprintf(stderr, "\n\n");
            etiLog.level(info) << m.framecount << " DAB frames encoded";
            etiLog.level(info) << ((float)m.framecount * 0.024f) << " seconds encoded";

            m.data.setLength(0);
        }
    }

    etiLog.level(info) << "Terminating";
    return ret;
}

run_modulator_state_t run_modulator(modulator_data& m)
{
    auto ret = run_modulator_state_t::failure;
    try {
        while (running) {

            int framesize;

            PDEBUG("*****************************************\n");
            PDEBUG("* Starting main loop\n");
            PDEBUG("*****************************************\n");
            while ((framesize = m.inputReader->GetNextFrame(m.data.getData())) > 0) {
                if (!running) {
                    break;
                }

                m.framecount++;

                PDEBUG("*****************************************\n");
                PDEBUG("* Read frame %lu\n", m.framecount);
                PDEBUG("*****************************************\n");

                const int eti_bytes_read = m.etiReader->loadEtiData(m.data);
                if ((size_t)eti_bytes_read != m.data.getLength()) {
                    etiLog.level(error) << "ETI frame incompletely read";
                    throw std::runtime_error("ETI read error");
                }

                m.flowgraph->run();

                /* Check every once in a while if the remote control
                 * is still working */
                if ((m.framecount % 250) == 0) {
                    rcs.check_faults();
                }
            }
            if (framesize == 0) {
                etiLog.level(info) << "End of file reached.";
            }
            else {
                etiLog.level(error) << "Input read error.";
            }
            running = 0;
            ret = run_modulator_state_t::normal_end;
        }
    }
    catch (const zmq_input_overflow& e) {
        // The ZeroMQ input has overflowed its buffer
        etiLog.level(warn) << e.what();
        ret = run_modulator_state_t::again;
    }
    catch (const FrameMultiplexerError& e) {
        // The FrameMultiplexer saw an error or a change in the size of a
        // subchannel. This can be due to a multiplex reconfiguration.
        etiLog.level(warn) << e.what();
        ret = run_modulator_state_t::reconfigure;
    }
    catch (const std::exception& e) {
        etiLog.level(error) << "Exception caught: " << e.what();
        ret = run_modulator_state_t::failure;
    }

    return ret;
}

int main(int argc, char* argv[])
{
    // Set timezone to UTC
    setenv("TZ", "", 1);
    tzset();

    try {
        return launch_modulator(argc, argv);
    }
    catch (std::invalid_argument& e) {
        std::string what(e.what());
        if (not what.empty()) {
            std::cerr << "Modulator error: " << what << std::endl;
        }
    }
    catch (std::runtime_error& e) {
        std::cerr << "Modulator runtime error: " << e.what() << std::endl;
    }
}

