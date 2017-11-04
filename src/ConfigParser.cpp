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

#include <cstdint>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "ConfigParser.h"
#include "porting.h"
#include "Utils.h"
#include "Log.h"
#include "DabModulator.h"
#include "output/SDR.h"


using namespace std;

static GainMode parse_gainmode(const std::string &gainMode_setting)
{
    string gainMode_minuscule(gainMode_setting);
    std::transform(gainMode_minuscule.begin(), gainMode_minuscule.end(),
            gainMode_minuscule.begin(), ::tolower);

    if (gainMode_minuscule == "0" or gainMode_minuscule == "fix") {
        return GainMode::GAIN_FIX;
    }
    else if (gainMode_minuscule == "1" or gainMode_minuscule == "max") {
        return GainMode::GAIN_MAX;
    }
    else if (gainMode_minuscule == "2" or gainMode_minuscule == "var") {
        return GainMode::GAIN_VAR;
    }

    cerr << "Modulator gainmode setting '" << gainMode_setting <<
        "' not recognised." << endl;
    throw std::runtime_error("Configuration error");
}

static void parse_configfile(
        const std::string& configuration_file,
        mod_settings_t& mod_settings)
{
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
    mod_settings.gainmodeVariance = pt.get("modulator.normalise_variance",
            mod_settings.gainmodeVariance);

    mod_settings.dabMode = pt.get("modulator.mode", mod_settings.dabMode);
    mod_settings.clockRate = pt.get("modulator.dac_clk_rate", (size_t)0);
    mod_settings.digitalgain = pt.get("modulator.digital_gain", mod_settings.digitalgain);
    mod_settings.outputRate = pt.get("modulator.rate", mod_settings.outputRate);

    // FIR Filter parameters:
    if (pt.get("firfilter.enabled", 0) == 1) {
        mod_settings.filterTapsFilename =
            pt.get<std::string>("firfilter.filtertapsfile", "default");
    }

    // Poly coefficients:
    if (pt.get("poly.enabled", 0) == 1) {
        mod_settings.polyCoefFilename =
            pt.get<std::string>("poly.polycoeffile", "dpd/poly.coef");

        mod_settings.polyNumThreads =
            pt.get<int>("poly.num_threads", 0);
    }

    // Crest factor reduction
    if (pt.get("cfr.enabled", 0) == 1) {
        mod_settings.enableCfr = true;
        mod_settings.cfrClip = pt.get<float>("cfr.clip");
        mod_settings.cfrErrorClip = pt.get<float>("cfr.error_clip");
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
        Output::SDRDeviceConfig sdr_device_config;

        string device = pt.get("uhdoutput.device", "");
        const auto usrpType = pt.get("uhdoutput.type", "");
        if (usrpType != "") {
            if (not device.empty()) {
                device += ",";
            }
            device += "type=" + usrpType;
        }
        sdr_device_config.device = device;

        sdr_device_config.subDevice = pt.get("uhdoutput.subdevice", "");
        sdr_device_config.masterClockRate = pt.get<long>("uhdoutput.master_clock_rate", 0);

        if (sdr_device_config.device.find("master_clock_rate") != std::string::npos) {
            std::cerr << "Warning:"
                "setting master_clock_rate in [uhd] device is deprecated !\n";
        }

        if (sdr_device_config.device.find("type=") != std::string::npos) {
            std::cerr << "Warning:"
                "setting type in [uhd] device is deprecated !\n";
        }

        sdr_device_config.txgain = pt.get("uhdoutput.txgain", 0.0);
        sdr_device_config.rxgain = pt.get("uhdoutput.rxgain", 0.0);
        sdr_device_config.frequency = pt.get<double>("uhdoutput.frequency", 0);
        std::string chan = pt.get<std::string>("uhdoutput.channel", "");
        sdr_device_config.dabMode = mod_settings.dabMode;

        if (sdr_device_config.frequency == 0 && chan == "") {
            std::cerr << "       UHD output enabled, but neither frequency nor channel defined.\n";
            throw std::runtime_error("Configuration error");
        }
        else if (sdr_device_config.frequency == 0) {
            sdr_device_config.frequency = parseChannel(chan);
        }
        else if (sdr_device_config.frequency != 0 && chan != "") {
            std::cerr << "       UHD output: cannot define both frequency and channel.\n";
            throw std::runtime_error("Configuration error");
        }

        sdr_device_config.lo_offset = pt.get<double>("uhdoutput.lo_offset", 0);

        sdr_device_config.refclk_src = pt.get("uhdoutput.refclk_source", "internal");
        sdr_device_config.pps_src = pt.get("uhdoutput.pps_source", "none");
        sdr_device_config.pps_polarity = pt.get("uhdoutput.pps_polarity", "pos");

        std::string behave = pt.get("uhdoutput.behaviour_refclk_lock_lost", "ignore");

        if (behave == "crash") {
            sdr_device_config.refclk_lock_loss_behaviour = Output::CRASH;
        }
        else if (behave == "ignore") {
            sdr_device_config.refclk_lock_loss_behaviour = Output::IGNORE;
        }
        else {
            std::cerr << "Error: UHD output: behaviour_refclk_lock_lost invalid." << std::endl;
            throw std::runtime_error("Configuration error");
        }

        sdr_device_config.maxGPSHoldoverTime = pt.get("uhdoutput.max_gps_holdover_time", 0);

        sdr_device_config.dpdFeedbackServerPort = pt.get<long>("uhdoutput.dpd_port", 0);

        mod_settings.sdr_device_config = sdr_device_config;
        mod_settings.useUHDOutput = 1;
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (output_selected == "soapysdr") {
        auto& outputsoapy_conf = mod_settings.sdr_device_config;
        outputsoapy_conf.device = pt.get("soapyoutput.device", "");
        outputsoapy_conf.masterClockRate = pt.get<long>("soapyoutput.master_clock_rate", 0);

        outputsoapy_conf.txgain = pt.get("soapyoutput.txgain", 0.0);
        outputsoapy_conf.lo_offset = pt.get<double>("soapyoutput.lo_offset", 0.0);
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
    mod_settings.sdr_device_config.enableSync = (pt.get("delaymanagement.synchronous", 0) == 1);
    mod_settings.sdr_device_config.muteNoTimestamps = (pt.get("delaymanagement.mutenotimestamps", 0) == 1);
    if (mod_settings.sdr_device_config.enableSync) {
        std::string delay_mgmt = pt.get<std::string>("delaymanagement.management", "");
        std::string fixedoffset = pt.get<std::string>("delaymanagement.fixedoffset", "");
        std::string offset_filename = pt.get<std::string>("delaymanagement.dynamicoffsetfile", "");

        if (not(delay_mgmt.empty() and fixedoffset.empty() and offset_filename.empty())) {
            std::cerr << "Warning: you are using the old config syntax for the offset management.\n";
            std::cerr << "         Please see the example.ini configuration for the new settings.\n";
        }

        try {
            mod_settings.tist_offset_s = pt.get<double>("delaymanagement.offset");
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


void parse_args(int argc, char **argv, mod_settings_t& mod_settings)
{
    bool use_configuration_cmdline = false;
    bool use_configuration_file = false;
    std::string configuration_file;

    // No argument given ? You can't be serious ! Show usage.
    if (argc == 1) {
        printUsage(argv[0]);
        throw std::invalid_argument("Invalid command line options");
    }

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
            mod_settings.sdr_device_config.frequency = strtof(optarg, NULL);
#endif
            break;
        case 'g':
            mod_settings.gainMode = parse_gainmode(optarg);
            break;
        case 'G':
#if defined(HAVE_OUTPUT_UHD)
            mod_settings.sdr_device_config.txgain = strtod(optarg, NULL);
#endif
            break;
        case 'l':
            mod_settings.loop = true;
            break;
        case 'o':
            mod_settings.tist_offset_s = strtod(optarg, NULL);
#if defined(HAVE_OUTPUT_UHD)
            mod_settings.sdr_device_config.enableSync = true;
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
            mod_settings.sdr_device_config.device = optarg;
            mod_settings.sdr_device_config.refclk_src = "internal";
            mod_settings.sdr_device_config.pps_src = "none";
            mod_settings.sdr_device_config.pps_polarity = "pos";
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
            throw std::invalid_argument("Invalid command line options");
        }
    }

    // If only one argument is given, interpret as configuration file name
    if (argc == 2) {
        use_configuration_file = true;
        configuration_file = argv[1];
    }

    if (use_configuration_file && use_configuration_cmdline) {
        fprintf(stderr, "Warning: configuration file and command "
                        "line parameters are defined:\n\t"
                        "Command line parameters override settings "
                        "in the configuration file !\n");
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
        etiLog.level(error) << "Received invalid command line arguments";
        throw std::invalid_argument("Invalid command line options");
    }

    if (use_configuration_file) {
        parse_configfile(configuration_file, mod_settings);
    }
}

