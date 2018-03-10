/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

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
    std::shared_ptr<InputReader> inputReader;
    Buffer data;
    uint64_t framecount = 0;

    Flowgraph* flowgraph = nullptr;
    EtiReader* etiReader = nullptr;
};

enum class run_modulator_state_t {
    failure,    // Corresponds to all failures
    normal_end, // Number of frames to modulate was reached
    again,      // Restart the modulator part
    reconfigure // Some sort of change of configuration we cannot handle happened
};

static run_modulator_state_t run_modulator(modulator_data& m);

static void printModSettings(const mod_settings_t& mod_settings)
{
    stringstream ss;
    // Print settings
    ss << "Input\n";
    ss << "  Type: " << mod_settings.inputTransport << "\n";
    ss << "  Source: " << mod_settings.inputName << "\n";

    ss << "Output\n";

    if (mod_settings.useFileOutput) {
        ss << "  Name: " << mod_settings.outputName << "\n";
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (mod_settings.useUHDOutput) {
        ss << " UHD\n" <<
            "  Device: " << mod_settings.sdr_device_config.device << "\n" <<
            "  Subdevice: " <<
                mod_settings.sdr_device_config.subDevice << "\n" <<
            "  master_clock_rate: " <<
                mod_settings.sdr_device_config.masterClockRate << "\n" <<
            "  refclk: " <<
                mod_settings.sdr_device_config.refclk_src << "\n" <<
            "  pps source: " <<
                mod_settings.sdr_device_config.pps_src << "\n";
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (mod_settings.useSoapyOutput) {
        ss << " SoapySDR\n"
            "  Device: " << mod_settings.sdr_device_config.device << "\n" <<
            "  master_clock_rate: " <<
                mod_settings.sdr_device_config.masterClockRate << "\n";
    }
#endif
    else if (mod_settings.useZeroMQOutput) {
        ss << " ZeroMQ\n" <<
            "  Listening on: " << mod_settings.outputName << "\n" <<
            "  Socket type : " << mod_settings.zmqOutputSocketType << "\n";
    }

    ss << "  Sampling rate: ";
    if (mod_settings.outputRate > 1000) {
        if (mod_settings.outputRate > 1000000) {
            ss << std::fixed << std::setprecision(4) <<
                mod_settings.outputRate / 1000000.0 <<
                " MHz\n";
        }
        else {
            ss << std::fixed << std::setprecision(4) <<
                mod_settings.outputRate / 1000.0 <<
                " kHz\n";
        }
    }
    else {
        ss << std::fixed << std::setprecision(4) <<
            mod_settings.outputRate << " Hz\n";
    }
    fprintf(stderr, "%s", ss.str().c_str());
}

static shared_ptr<ModOutput> prepare_output(
        mod_settings_t& s)
{
    shared_ptr<ModOutput> output;

    if (s.useFileOutput) {
        if (s.fileOutputFormat == "complexf") {
            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else if (s.fileOutputFormat == "complexf_normalised") {
            if (s.gainMode == GainMode::GAIN_FIX)
                s.normalise = 1.0f / normalise_factor_file_fix;
            else if (s.gainMode == GainMode::GAIN_MAX)
                s.normalise = 1.0f / normalise_factor_file_max;
            else if (s.gainMode == GainMode::GAIN_VAR)
                s.normalise = 1.0f / normalise_factor_file_var;
            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else if (s.fileOutputFormat == "s8" or
                s.fileOutputFormat == "u8") {
            // We must normalise the samples to the interval [-127.0; 127.0]
            // The formatconverter will add 127 for u8 so that it ends up in
            // [0; 255]
            s.normalise = 127.0f / normalise_factor;

            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
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
        const string errstr = strerror(errno);
        throw runtime_error("Could not set signal handler: " + errstr);
    }

    printStartupInfo();

    mod_settings_t mod_settings;
    parse_args(argc, argv, mod_settings);

    etiLog.level(info) << "Configuration parsed. Starting up version " <<
#if defined(GITVERSION)
            GITVERSION;
#else
            VERSION;
#endif

    if (not (mod_settings.useFileOutput or
             mod_settings.useUHDOutput or
             mod_settings.useZeroMQOutput or
             mod_settings.useSoapyOutput)) {
        throw std::runtime_error("Configuration error: Output not specified");
    }

    printModSettings(mod_settings);

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
#ifdef HAVE_EDI
        EdiReader ediReader(mod_settings.tist_offset_s);
        EdiDecoder::ETIDecoder ediInput(ediReader, false);
        if (mod_settings.edi_max_delay_ms > 0.0f) {
            // setMaxDelay wants number of AF packets, which correspond to 24ms ETI frames
            ediInput.setMaxDelay(lroundf(mod_settings.edi_max_delay_ms / 24.0f));
        }
        EdiUdpInput ediUdpInput(ediInput);

        ediUdpInput.Open(mod_settings.inputName);
        if (not ediUdpInput.isEnabled()) {
            throw runtime_error("inputTransport is edi, but ediUdpInput is not enabled");
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

        size_t framecount = 0;

        bool first_frame = true;

        while (running) {
            while (not ediReader.isFrameReady()) {
                bool success = ediUdpInput.rxPacket();
                if (not success) {
                    running = 0;
                    break;
                }
            }

            if (first_frame) {
                if (ediReader.getFp() != 0) {
                    // Do not start the flowgraph before we get to FP 0
                    // to ensure all blocks are properly aligned.
                    ediReader.clearFrame();
                    continue;
                }
                else {
                    first_frame = false;
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
#else
        throw std::runtime_error("Unable to open input: "
                "EDI input transport selected, but not compiled in!");
#endif // HAVE_EDI
    }
    else {
        shared_ptr<InputReader> inputReader;

        if (mod_settings.inputTransport == "file") {
            auto inputFileReader = make_shared<InputFileReader>();

            // Opening ETI input file
            if (inputFileReader->Open(mod_settings.inputName, mod_settings.loop) == -1) {
                throw std::runtime_error("Unable to open input");
            }

            inputReader = inputFileReader;
        }
        else if (mod_settings.inputTransport == "zeromq") {
#if !defined(HAVE_ZEROMQ)
            throw std::runtime_error("Unable to open input: "
                    "ZeroMQ input transport selected, but not compiled in!");
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
        else {
            throw std::runtime_error("Unable to open input: "
                    "invalid input transport " + mod_settings.inputTransport + " selected!");
        }

        bool run_again = true;

        while (run_again) {
            Flowgraph flowgraph;

            modulator_data m;
            m.inputReader = inputReader;
            m.flowgraph = &flowgraph;
            m.data.setLength(6144);

            EtiReader etiReader(mod_settings.tist_offset_s);
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
                        // Keep the same inputReader, as there is no input buffer overflow
                        run_again = true;
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

            etiLog.level(info) << m.framecount << " DAB frames encoded";
            etiLog.level(info) << ((float)m.framecount * 0.024f) << " seconds encoded";
        }
    }

    etiLog.level(info) << "Terminating";
    return ret;
}

static run_modulator_state_t run_modulator(modulator_data& m)
{
    auto ret = run_modulator_state_t::failure;
    try {
        bool first_frame = true;
        int last_eti_fct = -1;

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

                if (first_frame) {
                    if (m.etiReader->getFp() != 0) {
                        // Do not start the flowgraph before we get to FP 0
                        // to ensure all blocks are properly aligned.
                        continue;
                    }
                    else {
                        first_frame = false;
                    }
                }

                // Check for ETI FCT continuity
                const unsigned expected_fct = (last_eti_fct + 1) % 250;
                const unsigned fct = m.etiReader->getFct();
                if (last_eti_fct != -1 and expected_fct != fct) {
                    etiLog.level(info) << "ETI FCT discontinuity, expected " <<
                        expected_fct << " received " << m.etiReader->getFct();
                    return run_modulator_state_t::again;
                }
                last_eti_fct = fct;

                m.flowgraph->run();

                /* Check every once in a while if the remote control
                 * is still working */
                if ((m.framecount % 250) == 0) {
                    rcs.check_faults();
                }
            }
            if (framesize == 0) {
                if (dynamic_pointer_cast<InputFileReader>(m.inputReader)) {
                    etiLog.level(info) << "End of file reached.";
                    running = 0;
                    ret = run_modulator_state_t::normal_end;
                }
#if defined(HAVE_ZEROMQ)
                else if (dynamic_pointer_cast<InputZeroMQReader>(m.inputReader)) {
                    /* An empty frame marks a timeout. We ignore it, but we are
                     * now able to handle SIGINT properly.
                     */
                }
#endif // defined(HAVE_ZEROMQ)
                else if (dynamic_pointer_cast<InputTcpReader>(m.inputReader)) {
                    /* Same as for ZeroMQ */
                }
                else {
                    throw logic_error("Unhandled framesize==0!");
                }
            }
            else {
                etiLog.level(error) << "Input read error.";
                running = 0;
                ret = run_modulator_state_t::normal_end;
            }
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

    return 1;
}

