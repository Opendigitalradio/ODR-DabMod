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

#include <cstdint>
#include <algorithm>

#include "INIReader.h"

#include "ConfigParser.h"
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
    INIReader pt(configuration_file);

    int line_err = pt.ParseError();

    if (line_err) {
        std::cerr << "Error, cannot read configuration file '" << configuration_file.c_str() << "'" << std::endl;
        std::cerr << "At line:       " << line_err << std::endl;
        throw std::runtime_error("Cannot read configuration file");
    }

    // remote controller interfaces:
    if (pt.GetInteger("remotecontrol.telnet", 0) == 1) {
        try {
            int telnetport = pt.GetInteger("remotecontrol.telnetport", 0);
            auto telnetrc = make_shared<RemoteControllerTelnet>(telnetport);
            rcs.add_controller(telnetrc);
        }
        catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cerr << "       telnet remote control enabled, but no telnetport defined.\n";
            throw std::runtime_error("Configuration error");
        }
    }
#if defined(HAVE_ZEROMQ)
    if (pt.GetInteger("remotecontrol.zmqctrl", 0) == 1) {
        try {
            std::string zmqCtrlEndpoint = pt.Get("remotecontrol.zmqctrlendpoint", "");
            auto zmqrc = make_shared<RemoteControllerZmq>(zmqCtrlEndpoint);
            rcs.add_controller(zmqrc);
        }
        catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cerr << "       zmq remote control enabled, but no endpoint defined.\n";
            throw std::runtime_error("Configuration error");
        }
    }
#endif

    // input params:
    if (pt.GetInteger("input.loop", 0) == 1) {
        mod_settings.loop = true;
    }

    mod_settings.inputTransport = pt.Get("input.transport", "file");
    mod_settings.inputMaxFramesQueued = pt.GetInteger("input.max_frames_queued",
            ZMQ_INPUT_MAX_FRAME_QUEUE);

    mod_settings.edi_max_delay_ms = pt.GetReal("input.edi_max_delay", 0.0f);

    mod_settings.inputName = pt.Get("input.source", "/dev/stdin");

    // log parameters:
    if (pt.GetInteger("log.syslog", 0) == 1) {
        etiLog.register_backend(make_shared<LogToSyslog>());
    }

    if (pt.GetInteger("log.filelog", 0) == 1) {
        std::string logfilename;
        try {
            logfilename = pt.Get("log.filename", "");
        }
        catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cerr << "       Configuration enables file log, but does not specify log filename\n";
            throw std::runtime_error("Configuration error");
        }

        etiLog.register_backend(make_shared<LogToFile>(logfilename));
    }

    std::string trace_filename = pt.Get("log.trace", "");
    if (not trace_filename.empty()) {
        etiLog.register_backend(make_shared<LogTracer>(trace_filename));
    }

    mod_settings.showProcessTime = pt.GetInteger("log.show_process_time",
            mod_settings.showProcessTime);

    // modulator parameters:
    const string gainMode_setting = pt.Get("modulator.gainmode", "var");
    mod_settings.gainMode = parse_gainmode(gainMode_setting);
    mod_settings.gainmodeVariance = pt.GetReal("modulator.normalise_variance",
            mod_settings.gainmodeVariance);

    mod_settings.dabMode = pt.GetInteger("modulator.mode", mod_settings.dabMode);
    mod_settings.clockRate = pt.GetInteger("modulator.dac_clk_rate", (size_t)0);
    mod_settings.digitalgain = pt.GetReal("modulator.digital_gain",
            mod_settings.digitalgain);

    mod_settings.outputRate = pt.GetInteger("modulator.rate", mod_settings.outputRate);
    mod_settings.ofdmWindowOverlap = pt.GetInteger("modulator.ofdmwindowing",
            mod_settings.ofdmWindowOverlap);

    // FIR Filter parameters:
    if (pt.GetInteger("firfilter.enabled", 0) == 1) {
        mod_settings.filterTapsFilename =
            pt.Get("firfilter.filtertapsfile", "default");
    }

    // Poly coefficients:
    if (pt.GetInteger("poly.enabled", 0) == 1) {
        mod_settings.polyCoefFilename =
            pt.Get("poly.polycoeffile", "dpd/poly.coef");

        mod_settings.polyNumThreads =
            pt.GetInteger("poly.num_threads", 0);
    }

    // Crest factor reduction
    if (pt.GetInteger("cfr.enabled", 0) == 1) {
        mod_settings.enableCfr = true;
        mod_settings.cfrClip = pt.GetReal("cfr.clip", 0.0);
        mod_settings.cfrErrorClip = pt.GetReal("cfr.error_clip", 0.0);
    }

    // Output options
    std::string output_selected = pt.Get("output.output", "");
    if(output_selected == "") {
        std::cerr << "Error:\n";
        std::cerr << "       Configuration does not specify output\n";
        throw std::runtime_error("Configuration error");
    }

    if (output_selected == "file") {
        mod_settings.outputName = pt.Get("fileoutput.filename", "");
        if(mod_settings.outputName == "") {
            std::cerr << "Error:\n";
            std::cerr << "       Configuration does not specify file name for file output\n";
            throw std::runtime_error("Configuration error");
        }
        mod_settings.fileOutputShowMetadata =
                (pt.GetInteger("fileoutput.show_metadata", 0) > 0);
        mod_settings.useFileOutput = true;

        mod_settings.fileOutputFormat = pt.Get("fileoutput.format",
                mod_settings.fileOutputFormat);
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (output_selected == "uhd") {
        Output::SDRDeviceConfig sdr_device_config;

        string device = pt.Get("uhdoutput.device", "");
        const auto usrpType = pt.Get("uhdoutput.type", "");
        if (usrpType != "") {
            if (not device.empty()) {
                device += ",";
            }
            device += "type=" + usrpType;
        }
        sdr_device_config.device = device;

        sdr_device_config.subDevice = pt.Get("uhdoutput.subdevice", "");
        sdr_device_config.masterClockRate = pt.GetInteger("uhdoutput.master_clock_rate", 0);

        if (sdr_device_config.device.find("master_clock_rate") != std::string::npos) {
            std::cerr << "Warning:"
                "setting master_clock_rate in [uhd] device is deprecated !\n";
        }

        if (sdr_device_config.device.find("type=") != std::string::npos) {
            std::cerr << "Warning:"
                "setting type in [uhd] device is deprecated !\n";
        }

        sdr_device_config.txgain = pt.GetReal("uhdoutput.txgain", 0.0);
        sdr_device_config.tx_antenna = pt.Get("uhdoutput.tx_antenna", "");
        sdr_device_config.rx_antenna = pt.Get("uhdoutput.rx_antenna", "RX2");
        sdr_device_config.rxgain = pt.GetReal("uhdoutput.rxgain", 0.0);
        sdr_device_config.frequency = pt.GetReal("uhdoutput.frequency", 0);
        sdr_device_config.bandwidth = pt.GetReal("uhdoutput.bandwidth", 0);
        std::string chan = pt.Get("uhdoutput.channel", "");
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

        sdr_device_config.lo_offset = pt.GetReal("uhdoutput.lo_offset", 0);

        sdr_device_config.refclk_src = pt.Get("uhdoutput.refclk_source", "internal");
        sdr_device_config.pps_src = pt.Get("uhdoutput.pps_source", "none");
        sdr_device_config.pps_polarity = pt.Get("uhdoutput.pps_polarity", "pos");

        std::string behave = pt.Get("uhdoutput.behaviour_refclk_lock_lost", "ignore");

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

        sdr_device_config.maxGPSHoldoverTime = pt.GetInteger("uhdoutput.max_gps_holdover_time", 0);

        sdr_device_config.dpdFeedbackServerPort = pt.GetInteger("uhdoutput.dpd_port", 0);

        mod_settings.sdr_device_config = sdr_device_config;
        mod_settings.useUHDOutput = true;
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (output_selected == "soapysdr") {
        auto& outputsoapy_conf = mod_settings.sdr_device_config;
        outputsoapy_conf.device = pt.Get("soapyoutput.device", "");
        outputsoapy_conf.masterClockRate = pt.GetInteger("soapyoutput.master_clock_rate", 0);

        outputsoapy_conf.txgain = pt.GetReal("soapyoutput.txgain", 0.0);
        outputsoapy_conf.tx_antenna = pt.Get("soapyoutput.tx_antenna", "");
        outputsoapy_conf.lo_offset = pt.GetReal("soapyoutput.lo_offset", 0.0);
        outputsoapy_conf.frequency = pt.GetReal("soapyoutput.frequency", 0);
        outputsoapy_conf.bandwidth = pt.GetReal("soapyoutput.bandwidth", 0);
        std::string chan = pt.Get("soapyoutput.channel", "");
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

        outputsoapy_conf.dpdFeedbackServerPort = pt.GetInteger("soapyoutput.dpd_port", 0);

        mod_settings.useSoapyOutput = true;
    }
#endif
#if defined(HAVE_LIMESDR)
    else if (output_selected == "limesdr") {
        auto& outputlime_conf = mod_settings.sdr_device_config;
        outputlime_conf.device = pt.Get("limeoutput.device", "");
        outputlime_conf.masterClockRate = pt.GetInteger("limeoutput.master_clock_rate", 0);
        outputlime_conf.txgain = pt.GetReal("limeoutput.txgain", 0.0);
        outputlime_conf.tx_antenna = pt.Get("limeoutput.tx_antenna", "");
        outputlime_conf.lo_offset = pt.GetReal("limeoutput.lo_offset", 0.0);
        outputlime_conf.frequency = pt.GetReal("limeoutput.frequency", 0);
        std::string chan = pt.Get("limeoutput.channel", "");
        outputlime_conf.dabMode = mod_settings.dabMode;
        outputlime_conf.upsample = pt.GetInteger("limeoutput.upsample", 1);

        if (outputlime_conf.frequency == 0 && chan == "") {
            std::cerr << "       Lime output enabled, but neither frequency nor channel defined.\n";
            throw std::runtime_error("Configuration error");
        }
        else if (outputlime_conf.frequency == 0) {
            outputlime_conf.frequency =  parseChannel(chan);
        }
        else if (outputlime_conf.frequency != 0 && chan != "") {
            std::cerr << "       Lime output: cannot define both frequency and channel.\n";
            throw std::runtime_error("Configuration error");
        }

        outputlime_conf.dpdFeedbackServerPort = pt.GetInteger("limeoutput.dpd_port", 0);

        mod_settings.useLimeOutput = true;
    }
#endif
#if defined(HAVE_ZEROMQ)
    else if (output_selected == "zmq") {
        mod_settings.outputName = pt.Get("zmqoutput.listen", "");
        mod_settings.zmqOutputSocketType = pt.Get("zmqoutput.socket_type", "");
        mod_settings.useZeroMQOutput = true;
    }
#endif
    else {
        std::cerr << "Error: Invalid output defined.\n";
        throw std::runtime_error("Configuration error");
    }

#if defined(HAVE_OUTPUT_UHD)
    mod_settings.sdr_device_config.enableSync = (pt.GetInteger("delaymanagement.synchronous", 0) == 1);
    mod_settings.sdr_device_config.muteNoTimestamps = (pt.GetInteger("delaymanagement.mutenotimestamps", 0) == 1);
    if (mod_settings.sdr_device_config.enableSync) {
        std::string delay_mgmt = pt.Get("delaymanagement.management", "");
        std::string fixedoffset = pt.Get("delaymanagement.fixedoffset", "");
        std::string offset_filename = pt.Get("delaymanagement.dynamicoffsetfile", "");

        if (not(delay_mgmt.empty() and fixedoffset.empty() and offset_filename.empty())) {
            std::cerr << "Warning: you are using the old config syntax for the offset management.\n";
            std::cerr << "         Please see the example.ini configuration for the new settings.\n";
        }

        try {
            mod_settings.tist_offset_s = pt.GetReal("delaymanagement.offset", 0.0);
        }
        catch (const std::exception &e) {
            std::cerr << "Error: delaymanagement: synchronous is enabled, but no offset defined!\n";
            throw std::runtime_error("Configuration error");
        }
    }

#endif

    /* Read TII parameters from config file */
    mod_settings.tiiConfig.enable = pt.GetInteger("tii.enable", 0);
    mod_settings.tiiConfig.comb = pt.GetInteger("tii.comb", 0);
    mod_settings.tiiConfig.pattern = pt.GetInteger("tii.pattern", 0);
    mod_settings.tiiConfig.old_variant = pt.GetInteger("tii.old_variant", 0);
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
                throw std::invalid_argument("Options -u and -f are mutually exclusive");
            }
#endif
            mod_settings.outputName = optarg;
            mod_settings.useFileOutput = true;
            break;
        case 'F':
            if (mod_settings.useFileOutput) {
                mod_settings.fileOutputFormat = optarg;
            }
#if defined(HAVE_OUTPUT_UHD)
            else if (mod_settings.useUHDOutput) {
                mod_settings.sdr_device_config.frequency = strtof(optarg, NULL);
            }
#endif
            else {
                throw std::invalid_argument("Cannot use -F before setting output!");
            }
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
                throw std::invalid_argument("Options -u and -f are mutually exclusive");
            }
            mod_settings.sdr_device_config.device = optarg;
            mod_settings.sdr_device_config.refclk_src = "internal";
            mod_settings.sdr_device_config.pps_src = "none";
            mod_settings.sdr_device_config.pps_polarity = "pos";
            mod_settings.useUHDOutput = true;
#else
            throw std::invalid_argument("Cannot select UHD output, not compiled in!");
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
            {
                string optstr(1, c);
                throw std::invalid_argument("Invalid command line option: -" + optstr);
            }
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
        string invalid = "Invalid arguments:";
        while (optind != argc) {
            invalid += argv[optind++];
        }
        printUsage(argv[0]);
        etiLog.level(error) << "Received invalid command line arguments: " + invalid;
        throw std::invalid_argument("Invalid command line options");
    }

    if (use_configuration_file) {
        parse_configfile(configuration_file, mod_settings);
    }
}

