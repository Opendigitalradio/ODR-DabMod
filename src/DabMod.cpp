/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2014
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

#include "Log.h"
#include "DabModulator.h"
#include "InputMemory.h"
#include "OutputFile.h"
#if defined(HAVE_OUTPUT_UHD)
#   include "OutputUHD.h"
#endif
#include "InputReader.h"
#include "PcDebug.h"
#include "TimestampDecoder.h"
#include "FIRFilter.h"
#include "RemoteControl.h"

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

#ifdef HAVE_NETINET_IN_H
#   include <netinet/in.h>
#endif

#ifdef HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
#endif


typedef std::complex<float> complexf;


bool running = true;

void signalHandler(int signalNb)
{
    PDEBUG("signalHandler(%i)\n", signalNb);

    running = false;
}


void printUsage(char* progName, FILE* out = stderr)
{
    fprintf(out, "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE,
#if defined(GITVERSION)
            GITVERSION,
#else
            VERSION,
#endif
            __DATE__, __TIME__);
    fprintf(out, "Usage with configuration file:\n");
    fprintf(out, "\t%s -C config_file.ini\n\n", progName);

    fprintf(out, "Usage with command line options:\n");
    fprintf(out, "\t%s"
            " [input]"
            " (-f filename | -u uhddevice -F frequency) "
            " [-G txgain]"
            " [-o offset]"
            " [-O offsetfile]"
            " [-T filter_taps_file]"
            " [-a gain]"
            " [-c clockrate]"
            " [-g gainMode]"
            " [-h]"
            " [-l]"
            " [-m dabMode]"
            " [-r samplingRate]"
            "\n", progName);
    fprintf(out, "Where:\n");
    fprintf(out, "input:         ETI input filename (default: stdin).\n");
    fprintf(out, "-f name:       Use file output with given filename. (use /dev/stdout for standard output)\n");
    fprintf(out, "-u device:     Use UHD output with given device string. (use "" for default device)\n");
    fprintf(out, "-F frequency:  Set the transmit frequency when using UHD output. (mandatory option when using UHD)\n");
    fprintf(out, "-G txgain:     Set the transmit gain for the UHD driver (default: 0)\n");
    fprintf(out, "-o:            (UHD only) Set the timestamp offset added to the timestamp in the ETI. The offset is a double.\n");
    fprintf(out, "-O:            (UHD only) Set the file containing the timestamp offset added to the timestamp in the ETI.\n"
                                 "The file is read every six seconds, and must contain a double value.\n");
    fprintf(out, "                  Specifying either -o or -O has two implications: It enables synchronous transmission,\n"
                 "                  requiring an external REFCLK and PPS signal and frames that do not contain a valid timestamp\n"
                 "                  get muted.\n\n");
    fprintf(out, "-T taps_file:  Enable filtering before the output, using the specified file containing the filter taps.\n");
    fprintf(out, "-a gain:       Apply digital amplitude gain.\n");
    fprintf(out, "-c rate:       Set the DAC clock rate and enable Cic Equalisation.\n");
    fprintf(out, "-g:            Set computation gain mode: "
            "%u FIX, %u MAX, %u VAR\n", GAIN_FIX, GAIN_MAX, GAIN_VAR);
    fprintf(out, "-h:            Print this help.\n");
    fprintf(out, "-l:            Loop file when reach end of file.\n");
    fprintf(out, "-m mode:       Set DAB mode: (0: auto, 1-4: force).\n");
    fprintf(out, "-r rate:       Set output sampling rate (default: 2048000).\n");
}


void printVersion(FILE *out = stderr)
{
    fprintf(out, "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE, VERSION, __DATE__, __TIME__);
    fprintf(out, "ODR-DabMod is copyright (C) Her Majesty the Queen in Right of Canada,\n"
            "    2009, 2010, 2011, 2012 Communications Research Centre (CRC).\n"
            "\n"
            "    This program is available free of charge and is licensed to you on a\n"
            "    non-exclusive basis; you may not redistribute it.\n"
            "\n"
            "    This program is provided \"AS IS\" in the hope that it will be useful, but\n"
            "    WITHOUT ANY WARRANTY with respect to its accurancy or usefulness; witout\n"
            "    even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR\n"
            "    PURPOSE and NONINFRINGEMENT.\n"
            "\n"
            "    In no event shall CRC be LIABLE for any LOSS, DAMAGE or COST that may be\n"
            "    incurred in connection with the use of this software.\n"
            "\n"
            "ODR-DabMod makes use of the following open source packages:\n"
            "    Kiss FFT v1.2.9 (Revised BSD) - http://kissfft.sourceforge.net/\n"
           );

}


int main(int argc, char* argv[])
{
    int ret = 0;
    bool loop = false;
    std::string inputName = "";
    std::string inputTransport = "file";

    std::string outputName;
    int useFileOutput = 0;
    int useUHDOutput = 0;

    uint64_t frame = 0;
    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 0;
    float digitalgain = 1.0f;
    float normalise = 1.0f;
    GainMode gainMode = GAIN_VAR;
    Buffer data;

    std::string filterTapsFilename = "";

    // Two configuration sources exist: command line and (new) INI file
    bool use_configuration_cmdline = false;
    bool use_configuration_file = false;
    std::string configuration_file;

#if defined(HAVE_OUTPUT_UHD)
    OutputUHDConfig outputuhd_conf;
#endif

    // To handle the timestamp offset of the modulator
    struct modulator_offset_config modconf;
    modconf.use_offset_file = false;
    modconf.use_offset_fixed = false;
    modconf.delay_calculation_pipeline_stages = 0;

    Flowgraph* flowgraph = NULL;
    DabModulator* modulator = NULL;
    InputMemory* input = NULL;
    ModOutput* output = NULL;

    BaseRemoteController* rc = NULL;

    Logger logger;
    InputFileReader inputFileReader(logger);
#if defined(HAVE_INPUT_ZEROMQ)
    InputZeroMQReader inputZeroMQReader(logger);
#endif
    InputReader* inputReader;

    signal(SIGINT, signalHandler);

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
                goto END_MAIN;
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
            gainMode = (GainMode)strtol(optarg, NULL, 0);
            break;
        case 'G':
#if defined(HAVE_OUTPUT_UHD)
            outputuhd_conf.txgain = (int)strtol(optarg, NULL, 10);
#endif
            break;
        case 'l':
            loop = true;
            break;
        case 'o':
            if (modconf.use_offset_file)
            {
                fprintf(stderr, "Options -o and -O are mutually exclusive\n");
                goto END_MAIN;
            }
            modconf.use_offset_fixed = true;
            modconf.offset_fixed = strtod(optarg, NULL);
#if defined(HAVE_OUTPUT_UHD)
            outputuhd_conf.enableSync = true;
#endif
            break;
        case 'O':
            if (modconf.use_offset_fixed)
            {
                fprintf(stderr, "Options -o and -O are mutually exclusive\n");
                goto END_MAIN;
            }
            modconf.use_offset_file = true;
            modconf.offset_filename = std::string(optarg);
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
                goto END_MAIN;
            }
            outputuhd_conf.device = optarg;
            useUHDOutput = 1;
#endif
            break;
        case 'V':
            printVersion();
            goto END_MAIN;
            break;
        case '?':
        case 'h':
            printUsage(argv[0]);
            goto END_MAIN;
            break;
        default:
            fprintf(stderr, "Option '%c' not coded yet!\n", c);
            ret = -1;
            goto END_MAIN;
        }
    }

    std::cerr << "ODR-DabMod version " <<
#if defined(GITVERSION)
            GITVERSION
#else
            VERSION
#endif
            << std::endl;

    if (use_configuration_file && use_configuration_cmdline) {
        fprintf(stderr, "Warning: configuration file and command line parameters are defined:\n\t"
                        "Command line parameters override settings in the configuration file !\n");
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
            fprintf(stderr, "Error, cannot read configuration file '%s'\n", configuration_file.c_str());
            goto END_MAIN;
        }

        // remote controller:
        if (pt.get("remotecontrol.telnet", 0) == 1) {
            try {
                int telnetport = pt.get<int>("remotecontrol.telnetport");
                RemoteControllerTelnet* telnetrc = new RemoteControllerTelnet(telnetport);
                rc = telnetrc;
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       telnet remote control enabled, but no telnetport defined.\n";
                goto END_MAIN;
            }
        }

        // input params:
        if (pt.get("input.loop", 0) == 1) {
            loop = true;
        }

        inputTransport = pt.get("input.transport", "file");
        inputName = pt.get("input.source", "/dev/stdin");

        // log parameters:
        if (pt.get("log.syslog", 0) == 1) {
            LogToSyslog* log_syslog = new LogToSyslog();
            logger.register_backend(log_syslog);
        }

        if (pt.get("log.filelog", 0) == 1) {
            std::string logfilename;
            try {
                 logfilename = pt.get<std::string>("log.filename");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration enables file log, but does not specify log filename\n";
                goto END_MAIN;
            }

            LogToFile* log_file = new LogToFile(logfilename);
            logger.register_backend(log_file);
        }


        // modulator parameters:
        gainMode = (GainMode)pt.get("modulator.gainmode", 0);
        dabMode = pt.get("modulator.mode", dabMode);
        clockRate = pt.get("modulator.dac_clk_rate", (size_t)0);
        digitalgain = pt.get("modulator.digital_gain", digitalgain);
        outputRate = pt.get("modulator.rate", outputRate);
        
        // FIR Filter parameters:
        if (pt.get("firfilter.enabled", 0) == 1) {
            try {
                filterTapsFilename = pt.get<std::string>("firfilter.filtertapsfile");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration enables firfilter, but does not specify filter taps file\n";
                goto END_MAIN;
            }
        }

        // Output options
        std::string output_selected;
        try {
             output_selected = pt.get<std::string>("output.output");
        }
        catch (std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cerr << "       Configuration does not specify output\n";
            goto END_MAIN;
        }

        if (output_selected == "file") {
            try {
                outputName = pt.get<std::string>("fileoutput.filename");
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Configuration does not specify file name for file output\n";
                goto END_MAIN;
            }
            useFileOutput = 1;
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

            outputuhd_conf.txgain = pt.get("uhdoutput.txgain", 0);
            outputuhd_conf.frequency = pt.get<double>("uhdoutput.frequency", 0);
            std::string chan = pt.get<std::string>("uhdoutput.channel", "");

            if (outputuhd_conf.frequency == 0 && chan == "") {
                std::cerr << "       UHD output enabled, but neither frequency nor channel defined.\n";
                goto END_MAIN;
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
                    goto END_MAIN;
                }
                outputuhd_conf.frequency = freq;
            }
            else if (outputuhd_conf.frequency != 0 && chan != "") {
                std::cerr << "       UHD output: cannot define both frequency and channel.\n";
                goto END_MAIN;
            }


            outputuhd_conf.refclk_src = pt.get("uhdoutput.refclk_source", "int");
            outputuhd_conf.pps_src = pt.get("uhdoutput.pps_source", "int");
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
                goto END_MAIN;
            }

            useUHDOutput = 1;
        }
#endif
        else {
            std::cerr << "Error: Invalid output defined.\n";
            goto END_MAIN;
        }

#if defined(HAVE_OUTPUT_UHD)
        outputuhd_conf.enableSync = (pt.get("delaymanagement.synchronous", 0) == 1);
        if (outputuhd_conf.enableSync) {
            try {
                std::string delay_mgmt = pt.get<std::string>("delaymanagement.management");
                if (delay_mgmt == "fixed") {
                    modconf.offset_fixed = pt.get<double>("delaymanagement.fixedoffset");
                    modconf.use_offset_fixed = true;
                }
                else if (delay_mgmt == "dynamic") {
                    modconf.offset_filename = pt.get<std::string>("delaymanagement.dynamicoffsetfile");
                    modconf.use_offset_file = true;
                }
                else {
                    throw std::runtime_error("invalid management value");
                }
            }
            catch (std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
                std::cerr << "       Synchronised transmission enabled, but delay management specification is incomplete.\n";
                goto END_MAIN;
            }
        }

        outputuhd_conf.muteNoTimestamps = (pt.get("delaymanagement.mutenotimestamps", 0) == 1);
#endif
    }
    if (!rc) {
        logger.level(warn) << "No Remote-Control started";
        rc = new RemoteControllerDummy();
    }


    logger.level(info) << "Starting up";

    if (!(modconf.use_offset_file || modconf.use_offset_fixed)) {
        logger.level(debug) << "No Modulator offset defined, setting to 0";
        modconf.use_offset_fixed = true;
        modconf.offset_fixed = 0;
    }

    // When using the FIRFilter, increase the modulator offset pipelining delay
    // by the correct amount
    if (filterTapsFilename != "") {
        modconf.delay_calculation_pipeline_stages += FIRFILTER_PIPELINE_DELAY;
    }

    // Setting ETI input filename
    if (inputName == "") {
        if (optind < argc) {
            inputName = argv[optind++];

            if (inputName.substr(0, 4) == "zmq+" &&
                inputName.find("://") != std::string::npos) {
                // if the name starts with zmq+XYZ://somewhere:port
                inputTransport = "zeromq";
            }
        }
        else {
            inputName = "/dev/stdin";
        }
    }

    // Checking unused arguments
    if (optind != argc) {
        fprintf(stderr, "Invalid arguments:");
        while (optind != argc) {
            fprintf(stderr, " %s", argv[optind++]);
        }
        fprintf(stderr, "\n");
        printUsage(argv[0]);
        ret = -1;
        logger.level(error) << "Received invalid command line arguments";
        goto END_MAIN;
    }

    if (!useFileOutput && !useUHDOutput) {
        logger.level(error) << "Output not specified";
        fprintf(stderr, "Must specify output !");
        goto END_MAIN;
    }

    // Print settings
    fprintf(stderr, "Input\n");
    fprintf(stderr, "  Type: %s\n", inputTransport.c_str());
    fprintf(stderr, "  Source: %s\n", inputName.c_str());
    fprintf(stderr, "Output\n");
#if defined(HAVE_OUTPUT_UHD)
    if (useUHDOutput) {
        fprintf(stderr, " UHD\n"
                        "  Device: %s\n"
                        "  Type: %s\n"
                        "  master_clock_rate: %ld\n",
                outputuhd_conf.device.c_str(),
                outputuhd_conf.usrpType.c_str(),
                outputuhd_conf.masterClockRate);
    }
    else if (useFileOutput) {
#else
    if (useFileOutput) {
#endif
        fprintf(stderr, "  Name: %s\n", outputName.c_str());
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

    if (inputTransport == "file") {
        // Opening ETI input file
        if (inputFileReader.Open(inputName, loop) == -1) {
            fprintf(stderr, "Unable to open input file!\n");
            logger.level(error) << "Unable to open input file!";
            ret = -1;
            goto END_MAIN;
        }

        inputReader = &inputFileReader;
    }
    else if (inputTransport == "zeromq") {
#if !defined(HAVE_INPUT_ZEROMQ)
        fprintf(stderr, "Error, ZeroMQ input transport selected, but not compiled in!\n");
        ret = -1;
        goto END_MAIN;
#else
        // The URL might start with zmq+tcp://
        if (inputName.substr(0, 4) == "zmq+") {
            inputZeroMQReader.Open(inputName.substr(4));
        }
        else {
            inputZeroMQReader.Open(inputName);
        }
        inputReader = &inputZeroMQReader;
#endif
    }
    else
    {
        fprintf(stderr, "Error, invalid input transport %s selected!\n", inputTransport.c_str());
        ret = -1;
        goto END_MAIN;
    }


    if (useFileOutput) {
        // Opening COFDM output file
        output = new OutputFile(outputName);
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (useUHDOutput) {

        /* UHD requires the input I and Q samples to be in the interval
         * [-1.0,1.0], otherwise they get truncated, which creates very
         * wide-spectrum spikes. Depending on the Transmission Mode, the
         * Gain Mode and the sample rate (and maybe other parameters), the
         * samples can have peaks up to about 48000. The value of 50000
         * should guarantee that with a digital gain of 1.0, UHD never clips
         * our samples.
         */
        normalise = 1.0f/50000.0f;

        outputuhd_conf.sampleRate = outputRate;
        try {
            output = new OutputUHD(outputuhd_conf, logger);
            ((OutputUHD*)output)->enrol_at(*rc);
        }
        catch (std::exception& e) {
            logger.level(error) << "UHD initialisation failed:" << e.what();
            goto END_MAIN;
        }
    }
#endif

    flowgraph = new Flowgraph();
    data.setLength(6144);
    input = new InputMemory(&data);
    modulator = new DabModulator(modconf, rc, logger, outputRate, clockRate,
            dabMode, gainMode, digitalgain, normalise, filterTapsFilename);
    flowgraph->connect(input, modulator);
    flowgraph->connect(modulator, output);

#if defined(HAVE_OUTPUT_UHD)
    if (useUHDOutput) {
        ((OutputUHD*)output)->setETIReader(modulator->getEtiReader());
    }
#endif

    inputReader->PrintInfo();

    try {
        while (running) {

            int framesize;

            PDEBUG("*****************************************\n");
            PDEBUG("* Starting main loop\n");
            PDEBUG("*****************************************\n");
            while ((framesize = inputReader->GetNextFrame(data.getData())) > 0) {
                if (!running) {
                    break;
                }

                frame++;

                PDEBUG("*****************************************\n");
                PDEBUG("* Read frame %lu\n", frame);
                PDEBUG("*****************************************\n");

                ////////////////////////////////////////////////////////////////
                // Proccessing data
                ////////////////////////////////////////////////////////////////
                flowgraph->run();

                /* Check every once in a while if the remote control
                 * is still working */
                if (rc && (frame % 250) == 0 && rc->fault_detected()) {
                    fprintf(stderr,
                            "Detected Remote Control fault, restarting it\n");
                    rc->restart();
                }
            }
            if (framesize == 0) {
                fprintf(stderr, "End of file reached.\n");
            }
            else {
                fprintf(stderr, "Input read error.\n");
            }
            running = false;
        }
    } catch (std::exception& e) {
        fprintf(stderr, "EXCEPTION: %s\n", e.what());
        ret = -1;
    }

END_MAIN:
    ////////////////////////////////////////////////////////////////////////
    // Cleaning things
    ////////////////////////////////////////////////////////////////////////
    fprintf(stderr, "\n\n");
    fprintf(stderr, "%lu DAB frames encoded\n", frame);
    fprintf(stderr, "%f seconds encoded\n", (float)frame * 0.024f);

    fprintf(stderr, "\nCleaning flowgraph...\n");
    delete flowgraph;

    // Cif
    fprintf(stderr, "\nCleaning buffers...\n");

    logger.level(info) << "Terminating";

    return ret;
}

