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
#if defined(HAVE_SOAPYSDR)
#   include "OutputSoapy.h"
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


/* UHD requires the input I and Q samples to be in the interval
 * [-1.0,1.0], otherwise they get truncated, which creates very
 * wide-spectrum spikes. Depending on the Transmission Mode, the
 * Gain Mode and the sample rate (and maybe other parameters), the
 * samples can have peaks up to about 48000. The value of 50000
 * should guarantee that with a digital gain of 1.0, UHD never clips
 * our samples.
 */
static const float normalise_factor = 50000.0f;

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

struct mod_settings_t {
    std::string outputName;
    int useZeroMQOutput = 0;
    std::string zmqOutputSocketType = "";
    int useFileOutput = 0;
    std::string fileOutputFormat = "complexf";
    int useUHDOutput = 0;
    int useSoapyOutput = 0;

    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 0;
    float digitalgain = 1.0f;
    float normalise = 1.0f;
    GainMode gainMode = GainMode::GAIN_VAR;

    bool loop = false;
    std::string inputName = "";
    std::string inputTransport = "file";
    unsigned inputMaxFramesQueued = ZMQ_INPUT_MAX_FRAME_QUEUE;
    float edi_max_delay_ms = 0.0f;

    tii_config_t tiiConfig;

    std::string filterTapsFilename = "";


#if defined(HAVE_OUTPUT_UHD)
    OutputUHDConfig outputuhd_conf;
#endif

#if defined(HAVE_SOAPYSDR)
    OutputSoapyConfig outputsoapy_conf;
#endif

};

static shared_ptr<ModOutput> prepare_output(
        mod_settings_t& s)
{
    shared_ptr<ModOutput> output;

    if (s.useFileOutput) {
        if (s.fileOutputFormat == "complexf") {
            output = make_shared<OutputFile>(s.outputName);
        }
        else if (s.fileOutputFormat == "s8") {
            // We must normalise the samples to the interval [-127.0; 127.0]
            s.normalise = 127.0f / normalise_factor;

            output = make_shared<OutputFile>(s.outputName);
        }
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (s.useUHDOutput) {
        s.normalise = 1.0f / normalise_factor;
        s.outputuhd_conf.sampleRate = s.outputRate;
        output = make_shared<OutputUHD>(s.outputuhd_conf);
        rcs.enrol((OutputUHD*)output.get());
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (s.useSoapyOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        s.outputsoapy_conf.sampleRate = s.outputRate;
        output = make_shared<OutputSoapy>(s.outputsoapy_conf);
        rcs.enrol((OutputSoapy*)output.get());
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
    mod_settings_t mod_settings;

    // Two configuration sources exist: command line and (new) INI file
    bool use_configuration_cmdline = false;
    bool use_configuration_file = false;
    std::string configuration_file;

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
            mod_settings.digitalgain = strtof(optarg, NULL);
            break;
        case 'c':
            mod_settings.clockRate = strtol(optarg, NULL, 0);
            break;
        case 'f':
#if defined(HAVE_OUTPUT_UHD)
            if (mod_settings.useUHDOutput) {
                fprintf(stderr, "Options -u and -f are mutually exclusive\n");
                throw std::invalid_argument("Invalid command line options");
            }
#endif
            mod_settings.outputName = optarg;
            mod_settings.useFileOutput = 1;
            break;
        case 'F':
#if defined(HAVE_OUTPUT_UHD)
            mod_settings.outputuhd_conf.frequency = strtof(optarg, NULL);
#endif
            break;
        case 'g':
            mod_settings.gainMode = parse_gainmode(optarg);
            break;
        case 'G':
#if defined(HAVE_OUTPUT_UHD)
            mod_settings.outputuhd_conf.txgain = strtod(optarg, NULL);
#endif
            break;
        case 'l':
            mod_settings.loop = true;
            break;
        case 'o':
            tist_offset_s = strtod(optarg, NULL);
#if defined(HAVE_OUTPUT_UHD)
            mod_settings.outputuhd_conf.enableSync = true;
#endif
            break;
        case 'm':
            mod_settings.dabMode = strtol(optarg, NULL, 0);
            break;
        case 'r':
            mod_settings.outputRate = strtol(optarg, NULL, 0);
            break;
        case 'T':
            mod_settings.filterTapsFilename = optarg;
            break;
        case 'u':
#if defined(HAVE_OUTPUT_UHD)
            if (mod_settings.useFileOutput) {
                fprintf(stderr, "Options -u and -f are mutually exclusive\n");
                throw std::invalid_argument("Invalid command line options");
            }
            mod_settings.outputuhd_conf.device = optarg;
            mod_settings.outputuhd_conf.refclk_src = "internal";
            mod_settings.outputuhd_conf.pps_src = "none";
            mod_settings.outputuhd_conf.pps_polarity = "pos";
            mod_settings.useUHDOutput = 1;
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
#if defined(HAVE_SOAPYSDR)
        "output_soapysdr " <<
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
            mod_settings.loop = true;
        }

        mod_settings.inputTransport = pt.get("input.transport", "file");
        mod_settings.inputMaxFramesQueued = pt.get("input.max_frames_queued",
                ZMQ_INPUT_MAX_FRAME_QUEUE);

        mod_settings.edi_max_delay_ms = pt.get("input.edi_max_delay", 0.0f);

        mod_settings.inputName = pt.get("input.source", "/dev/stdin");

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
        mod_settings.gainMode = parse_gainmode(gainMode_setting);

        mod_settings.dabMode = pt.get("modulator.mode", mod_settings.dabMode);
        mod_settings.clockRate = pt.get("modulator.dac_clk_rate", (size_t)0);
        mod_settings.digitalgain = pt.get("modulator.digital_gain", mod_settings.digitalgain);
        mod_settings.outputRate = pt.get("modulator.rate", mod_settings.outputRate);

        // FIR Filter parameters:
        if (pt.get("firfilter.enabled", 0) == 1) {
            mod_settings.filterTapsFilename =
                pt.get<std::string>("firfilter.filtertapsfile", "default");
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
                mod_settings.outputName = pt.get<std::string>("fileoutput.filename");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration does not specify file name for file output\n";
                throw std::runtime_error("Configuration error");
            }
            mod_settings.useFileOutput = 1;

            mod_settings.fileOutputFormat = pt.get("fileoutput.format", mod_settings.fileOutputFormat);
        }
#if defined(HAVE_OUTPUT_UHD)
        else if (output_selected == "uhd") {
            OutputUHDConfig outputuhd_conf;

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
            outputuhd_conf.dabMode = mod_settings.dabMode;

            if (outputuhd_conf.frequency == 0 && chan == "") {
                std::cerr << "       UHD output enabled, but neither frequency nor channel defined.\n";
                throw std::runtime_error("Configuration error");
            }
            else if (outputuhd_conf.frequency == 0) {
                outputuhd_conf.frequency = parseChannel(chan);
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

            mod_settings.outputuhd_conf = outputuhd_conf;
            mod_settings.useUHDOutput = 1;
        }
#endif
#if defined(HAVE_SOAPYSDR)
        else if (output_selected == "soapysdr") {
            auto& outputsoapy_conf = mod_settings.outputsoapy_conf;
            outputsoapy_conf.device = pt.get("soapyoutput.device", "");
            outputsoapy_conf.masterClockRate = pt.get<long>("soapyoutput.master_clock_rate", 0);

            outputsoapy_conf.txgain = pt.get("soapyoutput.txgain", 0.0);
            outputsoapy_conf.frequency = pt.get<double>("soapyoutput.frequency", 0);
            std::string chan = pt.get<std::string>("soapyoutput.channel", "");
            outputsoapy_conf.dabMode = mod_settings.dabMode;

            if (outputsoapy_conf.frequency == 0 && chan == "") {
                std::cerr << "       soapy output enabled, but neither frequency nor channel defined.\n";
                throw std::runtime_error("Configuration error");
            }
            else if (outputsoapy_conf.frequency == 0) {
                outputsoapy_conf.frequency =  parseChannel(chan);
            }
            else if (outputsoapy_conf.frequency != 0 && chan != "") {
                std::cerr << "       soapy output: cannot define both frequency and channel.\n";
                throw std::runtime_error("Configuration error");
            }

            mod_settings.useSoapyOutput = 1;
        }
#endif
#if defined(HAVE_ZEROMQ)
        else if (output_selected == "zmq") {
            mod_settings.outputName = pt.get<std::string>("zmqoutput.listen");
            mod_settings.zmqOutputSocketType = pt.get<std::string>("zmqoutput.socket_type");
            mod_settings.useZeroMQOutput = 1;
        }
#endif
        else {
            std::cerr << "Error: Invalid output defined.\n";
            throw std::runtime_error("Configuration error");
        }

#if defined(HAVE_OUTPUT_UHD)
        mod_settings.outputuhd_conf.enableSync = (pt.get("delaymanagement.synchronous", 0) == 1);
        mod_settings.outputuhd_conf.muteNoTimestamps = (pt.get("delaymanagement.mutenotimestamps", 0) == 1);
        if (mod_settings.outputuhd_conf.enableSync) {
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

#endif

        /* Read TII parameters from config file */
        mod_settings.tiiConfig.enable = pt.get("tii.enable", 0);
        mod_settings.tiiConfig.comb = pt.get("tii.comb", 0);
        mod_settings.tiiConfig.pattern = pt.get("tii.pattern", 0);
        mod_settings.tiiConfig.old_variant = pt.get("tii.old_variant", 0);
    }

    etiLog.level(info) << "Starting up version " <<
#if defined(GITVERSION)
            GITVERSION;
#else
            VERSION;
#endif


    // When using the FIRFilter, increase the modulator offset pipelining delay
    // by the correct amount
    if (not mod_settings.filterTapsFilename.empty()) {
        tist_delay_stages += FIRFILTER_PIPELINE_DELAY;
    }

    // Setting ETI input filename
    if (use_configuration_cmdline && mod_settings.inputName == "") {
        if (optind < argc) {
            mod_settings.inputName = argv[optind++];

            if (mod_settings.inputName.substr(0, 4) == "zmq+" &&
                mod_settings.inputName.find("://") != std::string::npos) {
                // if the name starts with zmq+XYZ://somewhere:port
                mod_settings.inputTransport = "zeromq";
            }
            else if (mod_settings.inputName.substr(0, 6) == "tcp://") {
                mod_settings.inputTransport = "tcp";
            }
            else if (mod_settings.inputName.substr(0, 6) == "udp://") {
                mod_settings.inputTransport = "edi";
            }
        }
        else {
            mod_settings.inputName = "/dev/stdin";
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

    if (not (mod_settings.useFileOutput or
             mod_settings.useUHDOutput or
             mod_settings.useZeroMQOutput or
             mod_settings.useSoapyOutput)) {
        etiLog.level(error) << "Output not specified";
        fprintf(stderr, "Must specify output !");
        throw std::runtime_error("Configuration error");
    }

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
                        "  Type: %s\n"
                        "  master_clock_rate: %ld\n"
                        "  refclk: %s\n"
                        "  pps source: %s\n",
                mod_settings.outputuhd_conf.device.c_str(),
                mod_settings.outputuhd_conf.usrpType.c_str(),
                mod_settings.outputuhd_conf.masterClockRate,
                mod_settings.outputuhd_conf.refclk_src.c_str(),
                mod_settings.outputuhd_conf.pps_src.c_str());
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (mod_settings.useSoapyOutput) {
        fprintf(stderr, " SoapySDR\n"
                        "  Device: %s\n"
                        "  master_clock_rate: %ld\n",
                mod_settings.outputsoapy_conf.device.c_str(),
                mod_settings.outputsoapy_conf.masterClockRate);
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
            fprintf(stderr, "%.4g MHz\n", mod_settings.outputRate / 1000000.0f);
        } else {
            fprintf(stderr, "%.4g kHz\n", mod_settings.outputRate / 1000.0f);
        }
    } else {
        fprintf(stderr, "%zu Hz\n", mod_settings.outputRate);
    }


    EdiReader ediReader(tist_offset_s, tist_delay_stages);
    EdiDecoder::ETIDecoder ediInput(ediReader, false);
    if (mod_settings.edi_max_delay_ms > 0.0f) {
        // setMaxDelay wants number of AF packets, which correspond to 24ms ETI frames
        ediInput.setMaxDelay(lroundf(mod_settings.edi_max_delay_ms / 24.0f));
    }
    EdiUdpInput ediUdpInput(ediInput);

    if (mod_settings.inputTransport == "file") {
        // Opening ETI input file
        if (inputFileReader.Open(mod_settings.inputName, mod_settings.loop) == -1) {
            fprintf(stderr, "Unable to open input file!\n");
            etiLog.level(error) << "Unable to open input file!";
            ret = -1;
            throw std::runtime_error("Unable to open input");
        }

        m.inputReader = &inputFileReader;
    }
    else if (mod_settings.inputTransport == "zeromq") {
#if !defined(HAVE_ZEROMQ)
        fprintf(stderr, "Error, ZeroMQ input transport selected, but not compiled in!\n");
        ret = -1;
        throw std::runtime_error("Unable to open input");
#else
        inputZeroMQReader->Open(mod_settings.inputName, mod_settings.inputMaxFramesQueued);
        m.inputReader = inputZeroMQReader.get();
#endif
    }
    else if (mod_settings.inputTransport == "tcp") {
        inputTcpReader->Open(mod_settings.inputName);
        m.inputReader = inputTcpReader.get();
    }
    else if (mod_settings.inputTransport == "edi") {
        ediUdpInput.Open(mod_settings.inputName);
    }
    else
    {
        fprintf(stderr, "Error, invalid input transport %s selected!\n", mod_settings.inputTransport.c_str());
        ret = -1;
        throw std::runtime_error("Unable to open input");
    }

    if (mod_settings.useFileOutput and mod_settings.fileOutputFormat == "s8") {
        format_converter = make_shared<FormatConverter>();
    }

    prepare_output(mod_settings);

    // Set thread priority to realtime
    if (int r = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for modulator:" << r;
    }
    set_thread_name("modulator");

    if (mod_settings.inputTransport == "edi") {
        if (not ediUdpInput.isEnabled()) {
            etiLog.level(error) << "inputTransport is edi, but ediUdpInput is not enabled";
            return -1;
        }
        Flowgraph flowgraph;

        auto modulator = make_shared<DabModulator>(
                ediReader,
                mod_settings.tiiConfig,
                mod_settings.outputRate,
                mod_settings.clockRate,
                mod_settings.dabMode,
                mod_settings.gainMode,
                mod_settings.digitalgain,
                mod_settings.normalise,
                mod_settings.filterTapsFilename);

        if (format_converter) {
            flowgraph.connect(modulator, format_converter);
            flowgraph.connect(format_converter, output);
        }
        else {
            flowgraph.connect(modulator, output);
        }

#if defined(HAVE_OUTPUT_UHD)
        if (mod_settings.useUHDOutput) {
            ((OutputUHD*)output.get())->setETISource(modulator->getEtiSource());
        }
#endif
#if defined(HAVE_SOAPYSDR)
        if (mod_settings.useSoapyOutput) {
            ((OutputSoapy*)output.get())->setETISource(modulator->getEtiSource());
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
                    etiReader,
                    mod_settings.tiiConfig,
                    mod_settings.outputRate,
                    mod_settings.clockRate,
                    mod_settings.dabMode,
                    mod_settings.gainMode,
                    mod_settings.digitalgain,
                    mod_settings.normalise,
                    mod_settings.filterTapsFilename);

            if (format_converter) {
                flowgraph.connect(modulator, format_converter);
                flowgraph.connect(format_converter, output);
            }
            else {
                flowgraph.connect(modulator, output);
            }

#if defined(HAVE_OUTPUT_UHD)
            if (mod_settings.useUHDOutput) {
                ((OutputUHD*)output.get())->setETISource(modulator->getEtiSource());
            }
#endif
#if defined(HAVE_SOAPYSDR)
            if (mod_settings.useSoapyOutput) {
                ((OutputSoapy*)output.get())->setETISource(modulator->getEtiSource());
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
                    if (mod_settings.inputTransport == "file") {
                        if (inputFileReader.Open(mod_settings.inputName, mod_settings.loop) == -1) {
                            etiLog.level(error) << "Unable to open input file!";
                            ret = 1;
                        }
                        else {
                            run_again = true;
                        }
                    }
                    else if (mod_settings.inputTransport == "zeromq") {
#if defined(HAVE_ZEROMQ)
                        run_again = true;
                        // Create a new input reader
                        inputZeroMQReader = make_shared<InputZeroMQReader>();
                        inputZeroMQReader->Open(mod_settings.inputName, mod_settings.inputMaxFramesQueued);
                        m.inputReader = inputZeroMQReader.get();
#endif
                    }
                    else if (mod_settings.inputTransport == "tcp") {
                        inputTcpReader = make_shared<InputTcpReader>();
                        inputTcpReader->Open(mod_settings.inputName);
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

