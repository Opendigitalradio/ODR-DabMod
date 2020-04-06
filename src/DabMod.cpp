/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <memory>
#include <complex>
#include <string>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <sys/stat.h>
#include <signal.h>

#if HAVE_NETINET_IN_H
#   include <netinet/in.h>
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
#include "output/Lime.h"
#include "OutputZeroMQ.h"
#include "InputReader.h"
#include "PcDebug.h"
#include "FIRFilter.h"
#include "RemoteControl.h"
#include "ConfigParser.h"

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
    // For ETI
    std::shared_ptr<InputReader> inputReader;
    std::shared_ptr<EtiReader> etiReader;

    // For EDI
    std::shared_ptr<EdiInput> ediInput;

    // Common to both EDI and EDI
    uint64_t framecount = 0;
    Flowgraph *flowgraph = nullptr;
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
#if defined(HAVE_LIMESDR)
    else if (mod_settings.useLimeOutput) {
        ss << " LimeSDR\n"
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
        else if (s.fileOutputFormat == "s16") {
            // We must normalise the samples to the interval [-32767.0; 32767.0]
            s.normalise = 32767.0f / normalise_factor;

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
#if defined(HAVE_LIMESDR)
    else if (s.useLimeOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto limedevice = make_shared<Output::Lime>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, limedevice);
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
             mod_settings.useSoapyOutput or
             mod_settings.useLimeOutput)) {
        throw std::runtime_error("Configuration error: Output not specified");
    }

    printModSettings(mod_settings);

    shared_ptr<FormatConverter> format_converter;
    if (mod_settings.useFileOutput and
            (mod_settings.fileOutputFormat == "s8" or
             mod_settings.fileOutputFormat == "u8" or
             mod_settings.fileOutputFormat == "s16")) {
        format_converter = make_shared<FormatConverter>(mod_settings.fileOutputFormat);
    }

    auto output = prepare_output(mod_settings);

    // Set thread priority to realtime
    if (int r = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for modulator:" << r;
    }

    shared_ptr<InputReader> inputReader;
    shared_ptr<EdiInput> ediInput;

    if (mod_settings.inputTransport == "edi") {
        ediInput = make_shared<EdiInput>(mod_settings.tist_offset_s, mod_settings.edi_max_delay_ms);

        ediInput->ediTransport.Open(mod_settings.inputName);
        if (not ediInput->ediTransport.isEnabled()) {
            throw runtime_error("inputTransport is edi, but ediTransport is not enabled");
        }
    }
    else if (mod_settings.inputTransport == "file") {
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
        rcs.enrol(inputZeroMQReader.get());
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
        Flowgraph flowgraph(mod_settings.showProcessTime);

        modulator_data m;
        m.ediInput = ediInput;
        m.inputReader = inputReader;
        m.flowgraph = &flowgraph;

        shared_ptr<DabModulator> modulator;
        if (inputReader) {
            m.etiReader = make_shared<EtiReader>(mod_settings.tist_offset_s);
            modulator = make_shared<DabModulator>(*m.etiReader, mod_settings);
        }
        else if (ediInput) {
            modulator = make_shared<DabModulator>(ediInput->ediReader, mod_settings);
        }

        rcs.enrol(modulator.get());

        if (format_converter) {
            flowgraph.connect(modulator, format_converter);
            flowgraph.connect(format_converter, output);
        }
        else {
            flowgraph.connect(modulator, output);
        }

        if (inputReader) {
            etiLog.level(info) << inputReader->GetPrintableInfo();
        }

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
                else if (auto in_zmq = dynamic_pointer_cast<InputZeroMQReader>(inputReader)) {
                    run_again = true;
                    // Create a new input reader
                    rcs.remove_controllable(in_zmq.get());
                    auto inputZeroMQReader = make_shared<InputZeroMQReader>();
                    inputZeroMQReader->Open(mod_settings.inputName, mod_settings.inputMaxFramesQueued);
                    rcs.enrol(inputZeroMQReader.get());
                    inputReader = inputZeroMQReader;
                }
#endif
                else if (dynamic_pointer_cast<InputTcpReader>(inputReader)) {
                    // Keep the same inputReader, as there is no input buffer overflow
                    run_again = true;
                }
                else if (ediInput) {
                    // In EDI, keep the same input
                    run_again = true;
                }
                break;
            case run_modulator_state_t::reconfigure:
                etiLog.level(warn) << "Detected change in ensemble configuration.";
                /* We can keep the input in this case */
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

    etiLog.level(info) << "Terminating";
    return ret;
}

struct zmq_input_timeout : public std::exception
{
    const char* what() const throw()
    {
        return "InputZMQ timeout";
    }
};

static run_modulator_state_t run_modulator(modulator_data& m)
{
    auto ret = run_modulator_state_t::failure;
    try {
        int last_eti_fct = -1;
        auto last_frame_received = chrono::steady_clock::now();
        Buffer data;
        if (m.inputReader) {
            data.setLength(6144);
        }

        while (running) {
            unsigned fct = 0;
            unsigned fp = 0;

            /* Load ETI data from the source */
            if (m.inputReader) {
                int framesize = m.inputReader->GetNextFrame(data.getData());

                if (framesize == 0) {
                    if (dynamic_pointer_cast<InputFileReader>(m.inputReader)) {
                        etiLog.level(info) << "End of file reached.";
                        running = 0;
                        ret = run_modulator_state_t::normal_end;
                        break;
                    }
#if defined(HAVE_ZEROMQ)
                    else if (dynamic_pointer_cast<InputZeroMQReader>(m.inputReader)) {
                        /* An empty frame marks a timeout. We ignore it, but we are
                         * now able to handle SIGINT properly.
                         *
                         * Also, we reconnect zmq every 10 seconds to avoid some
                         * issues, discussed in
                         * https://stackoverflow.com/questions/26112992/zeromq-pub-sub-on-unreliable-connection
                         *
                         * > It is possible that the PUB socket sees the error
                         * > while the SUB socket does not.
                         * >
                         * > The ZMTP RFC has a proposal for heartbeating that would
                         * > solve this problem.  The current best solution is for
                         * > PUB sockets to send heartbeats (e.g. 1 per second) when
                         * > traffic is low, and for SUB sockets to disconnect /
                         * > reconnect if they stop getting these.
                         *
                         * We don't need a heartbeat, because our application is constant frame rate,
                         * the frames themselves can act as heartbeats.
                         */

                        const auto now = chrono::steady_clock::now();
                        if (last_frame_received + chrono::seconds(10) < now) {
                            throw zmq_input_timeout();
                        }
                    }
#endif // defined(HAVE_ZEROMQ)
                    else if (dynamic_pointer_cast<InputTcpReader>(m.inputReader)) {
                        /* Same as for ZeroMQ */
                    }
                    else {
                        throw logic_error("Unhandled framesize==0!");
                    }
                    continue;
                }
                else if (framesize < 0) {
                    etiLog.level(error) << "Input read error.";
                    running = 0;
                    ret = run_modulator_state_t::normal_end;
                    break;
                }

                const int eti_bytes_read = m.etiReader->loadEtiData(data);
                if ((size_t)eti_bytes_read != data.getLength()) {
                    etiLog.level(error) << "ETI frame incompletely read";
                    throw std::runtime_error("ETI read error");
                }

                last_frame_received = chrono::steady_clock::now();

                fct = m.etiReader->getFct();
                fp = m.etiReader->getFp();
            }
            else if (m.ediInput) {
                while (running and not m.ediInput->ediReader.isFrameReady()) {
                    try {
                        bool packet_received = m.ediInput->ediTransport.rxPacket();
                        if (packet_received) {
                            last_frame_received = chrono::steady_clock::now();
                        }
                    }
                    catch (const std::runtime_error& e) {
                        etiLog.level(warn) << "EDI input: " << e.what();
                        running = 0;
                        break;
                    }

                    if (last_frame_received + chrono::seconds(10) < chrono::steady_clock::now()) {
                        etiLog.level(error) << "No EDI data received in 10 seconds.";
                        running = 0;
                        break;
                    }
                }

                if (!running) {
                    break;
                }

                fct = m.ediInput->ediReader.getFct();
                fp = m.ediInput->ediReader.getFp();
            }

            const unsigned expected_fct = (last_eti_fct + 1) % 250;
            if (last_eti_fct == -1) {
                if (fp != 0) {
                    // Do not start the flowgraph before we get to FP 0
                    // to ensure all blocks are properly aligned.
                    if (m.ediInput) {
                        m.ediInput->ediReader.clearFrame();
                    }
                    continue;
                }
                else {
                    last_eti_fct = fct;
                    m.framecount++;
                    m.flowgraph->run();
                }
            }
            else if (fct == expected_fct) {
                last_eti_fct = fct;
                m.framecount++;
                m.flowgraph->run();
            }
            else {
                etiLog.level(info) << "ETI FCT discontinuity, expected " <<
                    expected_fct << " received " << fct;
                if (m.ediInput) {
                    m.ediInput->ediReader.clearFrame();
                }
                return run_modulator_state_t::again;
            }

            if (m.ediInput) {
                m.ediInput->ediReader.clearFrame();
            }

            /* Check every once in a while if the remote control
             * is still working */
            if ((m.framecount % 250) == 0) {
                rcs.check_faults();
            }
        }
    }
    catch (const zmq_input_timeout&) {
        // The ZeroMQ input timeout
        etiLog.level(warn) << "Timeout";
        ret = run_modulator_state_t::again;
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
    catch (const std::invalid_argument& e) {
        std::string what(e.what());
        if (not what.empty()) {
            std::cerr << "Modulator error: " << what << std::endl;
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Modulator runtime error: " << e.what() << std::endl;
    }

    return 1;
}

