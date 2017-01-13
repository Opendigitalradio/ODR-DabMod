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
#if defined(HAVE_OUTPUT_UHD)
#   include "OutputUHD.h"
#endif
#include "OutputZeroMQ.h"
#include "InputReader.h"
#include "PcDebug.h"
#include "TimestampDecoder.h"
#include "FIRFilter.h"
#include "RemoteControl.h"

#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <complex>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>
#include <signal.h>

#if HAVE_NETINET_IN_H
#   include <netinet/in.h>
#endif

#if HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
#endif

#define ZMQ_INPUT_MAX_FRAME_QUEUE 500


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
        etiReader(nullptr),
        rcs(nullptr) {}

    InputReader* inputReader;
    Buffer data;
    uint64_t framecount;

    Flowgraph* flowgraph;
    EtiReader* etiReader;
    RemoteControllers* rcs;
};

enum class run_modulator_state_t {
    failure,    // Corresponds to all failures
    normal_end, // Number of frames to modulate was reached
    again,      // ZeroMQ overrun
    reconfigure // Some sort of change of configuration we cannot handle happened
};

run_modulator_state_t run_modulator(modulator_data& m);

static GainMode parse_gainmode(const std::string &gainMode_setting)
{
    string gainMode_minuscule(gainMode_setting);
    std::transform(gainMode_minuscule.begin(), gainMode_minuscule.end(), gainMode_minuscule.begin(), ::tolower);

    if (gainMode_minuscule == "0" or gainMode_minuscule == "fix") { return GainMode::GAIN_FIX; }
    else if (gainMode_minuscule == "1" or gainMode_minuscule == "max") { return GainMode::GAIN_MAX; }
    else if (gainMode_minuscule == "2" or gainMode_minuscule == "var") { return GainMode::GAIN_VAR; }

    cerr << "Modulator gainmode setting '" << gainMode_setting << "' not recognised." << endl;
    throw std::runtime_error("Configuration error");
}

int launch_modulator(int argc, char* argv[])
{
    int ret = 0;
    bool loop = false;
    std::string inputName = "";
    std::string inputTransport = "file";
    unsigned inputMaxFramesQueued = ZMQ_INPUT_MAX_FRAME_QUEUE;
    float edi_max_delay_ms = 0.0f;

    std::string outputName;
    int useZeroMQOutput = 0;
    std::string zmqOutputSocketType = "";
    int useFileOutput = 0;
    std::string fileOutputFormat = "complexf";
    int useUHDOutput = 0;

    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 0;
    float digitalgain = 1.0f;
    float normalise = 1.0f;
    GainMode gainMode = GainMode::GAIN_VAR;

    tii_config_t tiiConfig;

    /* UHD requires the input I and Q samples to be in the interval
     * [-1.0,1.0], otherwise they get truncated, which creates very
     * wide-spectrum spikes. Depending on the Transmission Mode, the
     * Gain Mode and the sample rate (and maybe other parameters), the
     * samples can have peaks up to about 48000. The value of 50000
     * should guarantee that with a digital gain of 1.0, UHD never clips
     * our samples.
     */
    const float normalise_factor = 50000.0f;

    std::string filterTapsFilename = "";

    // Two configuration sources exist: command line and (new) INI file
    bool use_configuration_cmdline = false;
    bool use_configuration_file = false;
    std::string configuration_file;

#if defined(HAVE_OUTPUT_UHD)
    OutputUHDConfig outputuhd_conf;
#endif

    modulator_data m;

    // To handle the timestamp offset of the modulator
    unsigned tist_delay_stages = 0;
    double   tist_offset_s = 0.0;

    auto flowgraph = make_shared<Flowgraph>();
    shared_ptr<FormatConverter> format_converter;
    shared_ptr<ModOutput> output;

    m.rcs = &rcs;

    bool run_again = true;

    InputFileReader inputFileReader;
#if defined(HAVE_ZEROMQ)
    auto inputZeroMQReader = make_shared<InputZeroMQReader>();
#endif

    auto inputTcpReader = make_shared<InputTcpReader>();

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &signalHandler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    // Set timezone to UTC
    setenv("TZ", "", 1);
    tzset();

    while (true) {
        int c = getopt(argc, argv, "a:C:c:f:F:g:G:hlm:o:O:r:T:u:V");
        if (c == -1) {
            break;
        }

        if (c != 'C') {
            use_configuration_cmdline = true;
        }

        switch (c) {
        case 'C':
            use_configuration_file = true;
            configuration_file = optarg;
            break;

        case 'a':
            digitalgain = strtof(optarg, NULL);
            break;
        case 'c':
            clockRate = strtol(optarg, NULL, 0);
            break;
        case 'f':
#if defined(HAVE_OUTPUT_UHD)
            if (useUHDOutput) {
                fprintf(stderr, "Options -u and -f are mutually exclusive\n");
                throw std::invalid_argument("Invalid command line options");
            }
#endif
            outputName = optarg;
            useFileOutput = 1;
            break;
        case 'F':
#if defined(HAVE_OUTPUT_UHD)
            outputuhd_conf.frequency = strtof(optarg, NULL);
#endif
            break;
        case 'g':
            gainMode = parse_gainmode(optarg);
            break;
        case 'G':
#if defined(HAVE_OUTPUT_UHD)
            outputuhd_conf.txgain = strtod(optarg, NULL);
#endif
            break;
        case 'l':
            loop = true;
            break;
        case 'o':
            tist_offset_s = strtod(optarg, NULL);
#if defined(HAVE_OUTPUT_UHD)
            outputuhd_conf.enableSync = true;
#endif
            break;
        case 'm':
            dabMode = strtol(optarg, NULL, 0);
            break;
        case 'r':
            outputRate = strtol(optarg, NULL, 0);
            break;
        case 'T':
            filterTapsFilename = optarg;
            break;
        case 'u':
#if defined(HAVE_OUTPUT_UHD)
            if (useFileOutput) {
                fprintf(stderr, "Options -u and -f are mutually exclusive\n");
                throw std::invalid_argument("Invalid command line options");
            }
            outputuhd_conf.device = optarg;
            outputuhd_conf.refclk_src = "internal";
            outputuhd_conf.pps_src = "none";
            outputuhd_conf.pps_polarity = "pos";
            useUHDOutput = 1;
#endif
            break;
        case 'V':
            printVersion();
            throw std::invalid_argument("");
            break;
        case '?':
        case 'h':
            printUsage(argv[0]);
            throw std::invalid_argument("");
            break;
        default:
            fprintf(stderr, "Option '%c' not coded yet!\n", c);
            ret = -1;
            throw std::invalid_argument("Invalid command line options");
        }
    }

    std::cerr << "ODR-DabMod version " <<
#if defined(GITVERSION)
            GITVERSION
#else
            VERSION
#endif
            << std::endl;

    std::cerr << "Compiled with features: " <<
#if defined(HAVE_ZEROMQ)
        "zeromq " <<
#endif
#if defined(HAVE_OUTPUT_UHD)
        "output_uhd " <<
#endif
#if defined(__FAST_MATH__)
        "fast-math" <<
#endif
        "\n";

    if (use_configuration_file && use_configuration_cmdline) {
        fprintf(stderr, "Warning: configuration file and command line parameters are defined:\n\t"
                        "Command line parameters override settings in the configuration file !\n");
    }

    // No argument given ? You can't be serious ! Show usage.
    if (argc == 1) {
        printUsage(argv[0]);
        throw std::invalid_argument("Invalid command line options");
    }

    // If only one argument is given, interpret as configuration file name
    if (argc == 2) {
        use_configuration_file = true;
        configuration_file = argv[1];
    }

    if (use_configuration_file) {
        // First read parameters from the file
        using boost::property_tree::ptree;
        ptree pt;

        try {
            read_ini(configuration_file, pt);
        }
        catch (boost::property_tree::ini_parser::ini_parser_error &e)
        {
            std::cerr << "Error, cannot read configuration file '" << configuration_file.c_str() << "'" << std::endl;
            std::cerr << "       " << e.what() << std::endl;
            throw std::runtime_error("Cannot read configuration file");
        }

        // remote controller:
        if (pt.get("remotecontrol.telnet", 0) == 1) {
            try {
                int telnetport = pt.get<int>("remotecontrol.telnetport");
                auto telnetrc = make_shared<RemoteControllerTelnet>(telnetport);
                rcs.add_controller(telnetrc);
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       telnet remote control enabled, but no telnetport defined.\n";
                throw std::runtime_error("Configuration error");
            }
        }

#if defined(HAVE_ZEROMQ)
        if (pt.get("remotecontrol.zmqctrl", 0) == 1) {
            try {
                std::string zmqCtrlEndpoint = pt.get("remotecontrol.zmqctrlendpoint", "");
                std::cerr << "ZmqCtrlEndpoint: " << zmqCtrlEndpoint << std::endl;
                auto zmqrc = make_shared<RemoteControllerZmq>(zmqCtrlEndpoint);
                rcs.add_controller(zmqrc);
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       zmq remote control enabled, but no endpoint defined.\n";
                throw std::runtime_error("Configuration error");
            }
        }
#endif

        // input params:
        if (pt.get("input.loop", 0) == 1) {
            loop = true;
        }

        inputTransport = pt.get("input.transport", "file");
        inputMaxFramesQueued = pt.get("input.max_frames_queued",
                ZMQ_INPUT_MAX_FRAME_QUEUE);

        edi_max_delay_ms = pt.get("input.edi_max_delay", 0.0f);

        inputName = pt.get("input.source", "/dev/stdin");

        // log parameters:
        if (pt.get("log.syslog", 0) == 1) {
            LogToSyslog* log_syslog = new LogToSyslog();
            etiLog.register_backend(log_syslog);
        }

        if (pt.get("log.filelog", 0) == 1) {
            std::string logfilename;
            try {
                 logfilename = pt.get<std::string>("log.filename");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration enables file log, but does not specify log filename\n";
                throw std::runtime_error("Configuration error");
            }

            LogToFile* log_file = new LogToFile(logfilename);
            etiLog.register_backend(log_file);
        }

        auto trace_filename = pt.get<std::string>("log.trace", "");
        if (not trace_filename.empty()) {
            LogTracer* tracer = new LogTracer(trace_filename);
            etiLog.register_backend(tracer);
        }


        // modulator parameters:
        const string gainMode_setting = pt.get("modulator.gainmode", "var");
        gainMode = parse_gainmode(gainMode_setting);

        dabMode = pt.get("modulator.mode", dabMode);
        clockRate = pt.get("modulator.dac_clk_rate", (size_t)0);
        digitalgain = pt.get("modulator.digital_gain", digitalgain);
        outputRate = pt.get("modulator.rate", outputRate);

        // FIR Filter parameters:
        if (pt.get("firfilter.enabled", 0) == 1) {
            filterTapsFilename = pt.get<std::string>("firfilter.filtertapsfile", "default");
        }

        // Output options
        std::string output_selected;
        try {
             output_selected = pt.get<std::string>("output.output");
        }
        catch (std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cerr << "       Configuration does not specify output\n";
            throw std::runtime_error("Configuration error");
        }

        if (output_selected == "file") {
            try {
                outputName = pt.get<std::string>("fileoutput.filename");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration does not specify file name for file output\n";
                throw std::runtime_error("Configuration error");
            }
            useFileOutput = 1;

            fileOutputFormat = pt.get("fileoutput.format", fileOutputFormat);
        }
#if defined(HAVE_OUTPUT_UHD)
        else if (output_selected == "uhd") {
            outputuhd_conf.device = pt.get("uhdoutput.device", "");
            outputuhd_conf.usrpType = pt.get("uhdoutput.type", "");
            outputuhd_conf.subDevice = pt.get("uhdoutput.subdevice", "");
            outputuhd_conf.masterClockRate = pt.get<long>("uhdoutput.master_clock_rate", 0);

            if (outputuhd_conf.device.find("master_clock_rate") != std::string::npos) {
                std::cerr << "Warning:"
                    "setting master_clock_rate in [uhd] device is deprecated !\n";
            }

            if (outputuhd_conf.device.find("type=") != std::string::npos) {
                std::cerr << "Warning:"
                    "setting type in [uhd] device is deprecated !\n";
            }

            outputuhd_conf.txgain = pt.get("uhdoutput.txgain", 0.0);
            outputuhd_conf.frequency = pt.get<double>("uhdoutput.frequency", 0);
            std::string chan = pt.get<std::string>("uhdoutput.channel", "");
            outputuhd_conf.dabMode = dabMode;

            if (outputuhd_conf.frequency == 0 && chan == "") {
                std::cerr << "       UHD output enabled, but neither frequency nor channel defined.\n";
                throw std::runtime_error("Configuration error");
            }
            else if (outputuhd_conf.frequency == 0) {
                double freq;
                if      (chan == "5A") freq = 174928000;
                else if (chan == "5B") freq = 176640000;
                else if (chan == "5C") freq = 178352000;
                else if (chan == "5D") freq = 180064000;
                else if (chan == "6A") freq = 181936000;
                else if (chan == "6B") freq = 183648000;
                else if (chan == "6C") freq = 185360000;
                else if (chan == "6D") freq = 187072000;
                else if (chan == "7A") freq = 188928000;
                else if (chan == "7B") freq = 190640000;
                else if (chan == "7C") freq = 192352000;
                else if (chan == "7D") freq = 194064000;
                else if (chan == "8A") freq = 195936000;
                else if (chan == "8B") freq = 197648000;
                else if (chan == "8C") freq = 199360000;
                else if (chan == "8D") freq = 201072000;
                else if (chan == "9A") freq = 202928000;
                else if (chan == "9B") freq = 204640000;
                else if (chan == "9C") freq = 206352000;
                else if (chan == "9D") freq = 208064000;
                else if (chan == "10A") freq = 209936000;
                else if (chan == "10B") freq = 211648000;
                else if (chan == "10C") freq = 213360000;
                else if (chan == "10D") freq = 215072000;
                else if (chan == "11A") freq = 216928000;
                else if (chan == "11B") freq = 218640000;
                else if (chan == "11C") freq = 220352000;
                else if (chan == "11D") freq = 222064000;
                else if (chan == "12A") freq = 223936000;
                else if (chan == "12B") freq = 225648000;
                else if (chan == "12C") freq = 227360000;
                else if (chan == "12D") freq = 229072000;
                else if (chan == "13A") freq = 230784000;
                else if (chan == "13B") freq = 232496000;
                else if (chan == "13C") freq = 234208000;
                else if (chan == "13D") freq = 235776000;
                else if (chan == "13E") freq = 237488000;
                else if (chan == "13F") freq = 239200000;
                else {
                    std::cerr << "       UHD output: channel " << chan << " does not exist in table\n";
                    throw std::out_of_range("UHD channel selection error");
                }
                outputuhd_conf.frequency = freq;
            }
            else if (outputuhd_conf.frequency != 0 && chan != "") {
                std::cerr << "       UHD output: cannot define both frequency and channel.\n";
                throw std::runtime_error("Configuration error");
            }


            outputuhd_conf.refclk_src = pt.get("uhdoutput.refclk_source", "internal");
            outputuhd_conf.pps_src = pt.get("uhdoutput.pps_source", "none");
            outputuhd_conf.pps_polarity = pt.get("uhdoutput.pps_polarity", "pos");

            std::string behave = pt.get("uhdoutput.behaviour_refclk_lock_lost", "ignore");

            if (behave == "crash") {
                outputuhd_conf.refclk_lock_loss_behaviour = CRASH;
            }
            else if (behave == "ignore") {
                outputuhd_conf.refclk_lock_loss_behaviour = IGNORE;
            }
            else {
                std::cerr << "Error: UHD output: behaviour_refclk_lock_lost invalid." << std::endl;
                throw std::runtime_error("Configuration error");
            }

            outputuhd_conf.maxGPSHoldoverTime = pt.get("uhdoutput.max_gps_holdover_time", 0);

            useUHDOutput = 1;
        }
#endif
#if defined(HAVE_ZEROMQ)
        else if (output_selected == "zmq") {
            outputName = pt.get<std::string>("zmqoutput.listen");
            zmqOutputSocketType = pt.get<std::string>("zmqoutput.socket_type");
            useZeroMQOutput = 1;
        }
#endif
        else {
            std::cerr << "Error: Invalid output defined.\n";
            throw std::runtime_error("Configuration error");
        }

#if defined(HAVE_OUTPUT_UHD)
        outputuhd_conf.enableSync = (pt.get("delaymanagement.synchronous", 0) == 1);
        if (outputuhd_conf.enableSync) {
            std::string delay_mgmt = pt.get<std::string>("delaymanagement.management", "");
            std::string fixedoffset = pt.get<std::string>("delaymanagement.fixedoffset", "");
            std::string offset_filename = pt.get<std::string>("delaymanagement.dynamicoffsetfile", "");

            if (not(delay_mgmt.empty() and fixedoffset.empty() and offset_filename.empty())) {
                std::cerr << "Warning: you are using the old config syntax for the offset management.\n";
                std::cerr << "         Please see the example.ini configuration for the new settings.\n";
            }

            try {
                tist_offset_s = pt.get<double>("delaymanagement.offset");
            }
            catch (std::exception &e) {
                std::cerr << "Error: delaymanagement: synchronous is enabled, but no offset defined!\n";
                throw std::runtime_error("Configuration error");
            }
        }

        outputuhd_conf.muteNoTimestamps = (pt.get("delaymanagement.mutenotimestamps", 0) == 1);
#endif

        /* Read TII parameters from config file */
        tiiConfig.enable  = pt.get("tii.enable", 0);
        tiiConfig.comb    = pt.get("tii.comb", 0);
        tiiConfig.pattern = pt.get("tii.pattern", 0);
    }

    etiLog.level(info) << "Starting up version " <<
#if defined(GITVERSION)
            GITVERSION;
#else
            VERSION;
#endif


    // When using the FIRFilter, increase the modulator offset pipelining delay
    // by the correct amount
    if (not filterTapsFilename.empty()) {
        tist_delay_stages += FIRFILTER_PIPELINE_DELAY;
    }

    // Setting ETI input filename
    if (use_configuration_cmdline && inputName == "") {
        if (optind < argc) {
            inputName = argv[optind++];

            if (inputName.substr(0, 4) == "zmq+" &&
                inputName.find("://") != std::string::npos) {
                // if the name starts with zmq+XYZ://somewhere:port
                inputTransport = "zeromq";
            }
            else if (inputName.substr(0, 6) == "tcp://") {
                inputTransport = "tcp";
            }
            else if (inputName.substr(0, 6) == "udp://") {
                inputTransport = "edi";
            }
        }
        else {
            inputName = "/dev/stdin";
        }
    }

    // Checking unused arguments
    if (use_configuration_cmdline && optind != argc) {
        fprintf(stderr, "Invalid arguments:");
        while (optind != argc) {
            fprintf(stderr, " %s", argv[optind++]);
        }
        fprintf(stderr, "\n");
        printUsage(argv[0]);
        ret = -1;
        etiLog.level(error) << "Received invalid command line arguments";
        throw std::invalid_argument("Invalid command line options");
    }

    if (!useFileOutput && !useUHDOutput && !useZeroMQOutput) {
        etiLog.level(error) << "Output not specified";
        fprintf(stderr, "Must specify output !");
        throw std::runtime_error("Configuration error");
    }

    // Print settings
    fprintf(stderr, "Input\n");
    fprintf(stderr, "  Type: %s\n", inputTransport.c_str());
    fprintf(stderr, "  Source: %s\n", inputName.c_str());
    fprintf(stderr, "Output\n");

    if (useFileOutput) {
        fprintf(stderr, "  Name: %s\n", outputName.c_str());
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (useUHDOutput) {
        fprintf(stderr, " UHD\n"
                        "  Device: %s\n"
                        "  Type: %s\n"
                        "  master_clock_rate: %ld\n"
                        "  refclk: %s\n"
                        "  pps source: %s\n",
                outputuhd_conf.device.c_str(),
                outputuhd_conf.usrpType.c_str(),
                outputuhd_conf.masterClockRate,
                outputuhd_conf.refclk_src.c_str(),
                outputuhd_conf.pps_src.c_str());
    }
#endif
    else if (useZeroMQOutput) {
        fprintf(stderr, " ZeroMQ\n"
                        "  Listening on: %s\n"
                        "  Socket type : %s\n",
                        outputName.c_str(),
                        zmqOutputSocketType.c_str());
    }

    fprintf(stderr, "  Sampling rate: ");
    if (outputRate > 1000) {
        if (outputRate > 1000000) {
            fprintf(stderr, "%.4g MHz\n", outputRate / 1000000.0f);
        } else {
            fprintf(stderr, "%.4g kHz\n", outputRate / 1000.0f);
        }
    } else {
        fprintf(stderr, "%zu Hz\n", outputRate);
    }


    EdiReader ediReader(tist_offset_s, tist_delay_stages);
    EdiDecoder::ETIDecoder ediInput(ediReader);
    if (edi_max_delay_ms > 0.0f) {
        // setMaxDelay wants number of AF packets, which correspond to 24ms ETI frames
        ediInput.setMaxDelay(lroundf(edi_max_delay_ms / 24.0f));
    }
    EdiUdpInput ediUdpInput(ediInput);

    if (inputTransport == "file") {
        // Opening ETI input file
        if (inputFileReader.Open(inputName, loop) == -1) {
            fprintf(stderr, "Unable to open input file!\n");
            etiLog.level(error) << "Unable to open input file!";
            ret = -1;
            throw std::runtime_error("Unable to open input");
        }

        m.inputReader = &inputFileReader;
    }
    else if (inputTransport == "zeromq") {
#if !defined(HAVE_ZEROMQ)
        fprintf(stderr, "Error, ZeroMQ input transport selected, but not compiled in!\n");
        ret = -1;
        throw std::runtime_error("Unable to open input");
#else
        inputZeroMQReader->Open(inputName, inputMaxFramesQueued);
        m.inputReader = inputZeroMQReader.get();
#endif
    }
    else if (inputTransport == "tcp") {
        inputTcpReader->Open(inputName);
        m.inputReader = inputTcpReader.get();
    }
    else if (inputTransport == "edi") {
        ediUdpInput.Open(inputName);
    }
    else
    {
        fprintf(stderr, "Error, invalid input transport %s selected!\n", inputTransport.c_str());
        ret = -1;
        throw std::runtime_error("Unable to open input");
    }

    if (useFileOutput) {
        if (fileOutputFormat == "complexf") {
            output = make_shared<OutputFile>(outputName);
        }
        else if (fileOutputFormat == "s8") {
            // We must normalise the samples to the interval [-127.0; 127.0]
            normalise = 127.0f / normalise_factor;

            format_converter = make_shared<FormatConverter>();

            output = make_shared<OutputFile>(outputName);
        }
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (useUHDOutput) {
        normalise = 1.0f / normalise_factor;
        outputuhd_conf.sampleRate = outputRate;
        output = make_shared<OutputUHD>(outputuhd_conf);
        rcs.enrol((OutputUHD*)output.get());
    }
#endif
#if defined(HAVE_ZEROMQ)
    else if (useZeroMQOutput) {
        /* We normalise the same way as for the UHD output */
        normalise = 1.0f / normalise_factor;
        if (zmqOutputSocketType == "pub") {
            output = make_shared<OutputZeroMQ>(outputName, ZMQ_PUB);
        }
        else if (zmqOutputSocketType == "rep") {
            output = make_shared<OutputZeroMQ>(outputName, ZMQ_REP);
        }
        else {
            std::stringstream ss;
            ss << "ZeroMQ output socket type " << zmqOutputSocketType << " invalid";
            throw std::invalid_argument(ss.str());
        }
    }
#endif

    // Set thread priority to realtime
    if (int r = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for modulator:" << r;
    }
    set_thread_name("modulator");

    if (inputTransport == "edi") {
        if (not ediUdpInput.isEnabled()) {
            etiLog.level(error) << "inputTransport is edi, but ediUdpInput is not enabled";
            return -1;
        }
        Flowgraph flowgraph;

        auto modulator = make_shared<DabModulator>(
                ediReader, tiiConfig, outputRate, clockRate,
                dabMode, gainMode, digitalgain, normalise,
                filterTapsFilename);

        if (format_converter) {
            flowgraph.connect(modulator, format_converter);
            flowgraph.connect(format_converter, output);
        }
        else {
            flowgraph.connect(modulator, output);
        }

#if defined(HAVE_OUTPUT_UHD)
        if (useUHDOutput) {
            ((OutputUHD*)output.get())->setETISource(modulator->getEtiSource());
        }
#endif

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
        while (run_again) {
            Flowgraph flowgraph;

            m.flowgraph = &flowgraph;
            m.data.setLength(6144);

            EtiReader etiReader(tist_offset_s, tist_delay_stages);
            m.etiReader = &etiReader;

            auto input = make_shared<InputMemory>(&m.data);
            auto modulator = make_shared<DabModulator>(
                    etiReader, tiiConfig, outputRate, clockRate,
                    dabMode, gainMode, digitalgain, normalise,
                    filterTapsFilename);

            if (format_converter) {
                flowgraph.connect(modulator, format_converter);
                flowgraph.connect(format_converter, output);
            }
            else {
                flowgraph.connect(modulator, output);
            }

#if defined(HAVE_OUTPUT_UHD)
            if (useUHDOutput) {
                ((OutputUHD*)output.get())->setETISource(modulator->getEtiSource());
            }
#endif

            m.inputReader->PrintInfo();

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
                    if (inputTransport == "file") {
                        if (inputFileReader.Open(inputName, loop) == -1) {
                            etiLog.level(error) << "Unable to open input file!";
                            ret = 1;
                        }
                        else {
                            run_again = true;
                        }
                    }
                    else if (inputTransport == "zeromq") {
#if defined(HAVE_ZEROMQ)
                        run_again = true;
                        // Create a new input reader
                        inputZeroMQReader = make_shared<InputZeroMQReader>();
                        inputZeroMQReader->Open(inputName, inputMaxFramesQueued);
                        m.inputReader = inputZeroMQReader.get();
#endif
                    }
                    else if (inputTransport == "tcp") {
                        inputTcpReader = make_shared<InputTcpReader>();
                        inputTcpReader->Open(inputName);
                        m.inputReader = inputTcpReader.get();
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
    } catch (zmq_input_overflow& e) {
        // The ZeroMQ input has overflowed its buffer
        etiLog.level(warn) << e.what();
        ret = run_modulator_state_t::again;
    } catch (std::out_of_range& e) {
        // One of the DSP blocks has detected an invalid change
        // or value in some settings. This can be due to a multiplex
        // reconfiguration.
        etiLog.level(warn) << e.what();
        ret = run_modulator_state_t::reconfigure;
    } catch (std::exception& e) {
        etiLog.level(error) << "Exception caught: " << e.what();
        ret = run_modulator_state_t::failure;
    }

    return ret;
}

int main(int argc, char* argv[])
{
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

