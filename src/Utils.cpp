/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
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

#include "Utils.h"

#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <pthread.h>
#if defined(HAVE_PRCTL)
#  include <sys/prctl.h>
#endif


static void printHeader()
{
    std::cerr << "ODR-DabMod version " <<
#if defined(GITVERSION)
            GITVERSION
#else
            VERSION
#endif
            ", compiled at " << __DATE__ << ", " << __TIME__ << std::endl;

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
#if defined(HAVE_LIMESDR)
        "output_limesdr " <<
#endif
#if defined(__FAST_MATH__)
        "fast-math " <<
#endif
#if defined(__SSE__)
        "SSE " <<
#endif
        "\n";
}

void printUsage(const char* progName)
{
    FILE* out = stderr;
    fprintf(out, "Usage with configuration file:\n");
    fprintf(out, "\t%s config_file.ini\n\n", progName);

    fprintf(out, "Usage with command line options:\n");
    fprintf(out, "\t%s"
            " input"
            " (-f filename -F format | -u uhddevice -F frequency)"
            " [-o offset]"
            "\n\t"
            " [-G txgain]"
            " [-T filter_taps_file]"
            " [-a gain]"
            " [-c clockrate]"
            "\n\t"
            " [-g gainMode]"
            " [-m dabMode]"
            " [-r samplingRate]"
            " [-l]"
            " [-h]"
            "\n", progName);
    fprintf(out, "Where:\n");
    fprintf(out, "input:         ETI input filename (default: stdin), or\n");
    fprintf(out, "                  tcp://source:port for ETI-over-TCP input, or\n");
    fprintf(out, "                  udp://:port for EDI input.\n");
    fprintf(out, "-f name:       Use file output with given filename. (use /dev/stdout for standard output)\n");
    fprintf(out, "-F format:     Set the output format (see doc/example.ini for formats) for the file output.\n");
    fprintf(out, "-o:            Set the timestamp offset added to the timestamp in the ETI. The offset is a double.\n");
    fprintf(out, "                  Specifying this option has two implications: It enables synchronous transmission,\n"
                 "                  requiring an external REFCLK and PPS signal and frames that do not contain a valid timestamp\n"
                 "                  get muted.\n\n");
    fprintf(out, "-u device:     Use UHD output with given device string. (use "" for default device)\n");
    fprintf(out, "-F frequency:  Set the transmit frequency when using UHD output. (mandatory option when using UHD)\n");
    fprintf(out, "-G txgain:     Set the transmit gain for the UHD driver (default: 0)\n");
    fprintf(out, "-T taps_file:  Enable filtering before the output, using the specified file containing the filter taps.\n");
    fprintf(out, "               Use 'default' as taps_file to use the internal taps.\n");
    fprintf(out, "-a gain:       Apply digital amplitude gain.\n");
    fprintf(out, "-c rate:       Set the DAC clock rate and enable Cic Equalisation.\n");
    fprintf(out, "-g gainmode:   Set computation gain mode: fix, max or var\n");
    fprintf(out, "-m mode:       Set DAB mode: (0: auto, 1-4: force).\n");
    fprintf(out, "-r rate:       Set output sampling rate (default: 2048000).\n\n");
    fprintf(out, "-l:            Loop file when reach end of file.\n");
    fprintf(out, "-h:            Print this help.\n");
}


void printVersion(void)
{
    FILE *out = stderr;
    fprintf(out,
            "    ODR-DabMod is copyright (C) Her Majesty the Queen in Right of Canada,\n"
            "    2005 -- 2012 Communications Research Centre (CRC),\n"
            "     and\n"
            "    Copyright (C) 2023 Matthias P. Braendli, matthias.braendli@mpb.li\n"
            "\n"
            "    http://opendigitalradio.org\n"
            "\n"
            "    ODR-DabMod is free software: you can redistribute it and/or modify it\n"
            "    under the terms of the GNU General Public License as published by the\n"
            "    Free Software Foundation, either version 3 of the License, or (at your\n"
            "    option) any later version.\n"
            "\n"
            "    ODR-DabMod is distributed in the hope that it will be useful, but\n"
            "    WITHOUT ANY WARRANTY; without even the implied warranty of\n"
            "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
            "    General Public License for more details.\n"
            "\n"
            "    You should have received a copy of the GNU General Public License along\n"
            "    with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.\n"
            "\n"
           );
}

void printStartupInfo()
{
    printHeader();
}

void printModSettings(const mod_settings_t& mod_settings)
{
    std::stringstream ss;
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
#if defined(HAVE_DEXTER)
    else if (mod_settings.useDexterOutput) {
        ss << " PrecisionWave DEXTER\n";
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
#if defined(HAVE_BLADERF)
    else if (mod_settings.useBladeRFOutput) {
        ss << " BladeRF\n"
            "  Device: " << mod_settings.sdr_device_config.device << "\n" <<
            "  refclk: " << mod_settings.sdr_device_config.refclk_src << "\n";
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

int set_realtime_prio(int prio)
{
    // Set thread priority to realtime
    const int policy = SCHED_RR;
    sched_param sp;
    sp.sched_priority = sched_get_priority_min(policy) + prio;
    int ret = pthread_setschedparam(pthread_self(), policy, &sp);
    return ret;
}

void set_thread_name(const char *name)
{
#if defined(HAVE_PRCTL)
    prctl(PR_SET_NAME,name,0,0,0);
#endif
}

double parse_channel(const std::string& chan)
{
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
        std::cerr << "Channel " << chan << " does not exist in table\n";
        throw std::out_of_range("channel out of range");
    }
    return freq;
}

std::optional<std::string> convert_frequency_to_channel(double frequency)
{
    const int freq = round(frequency);
    std::string chan;
    if      (freq == 174928000) chan = "5A";
    else if (freq == 176640000) chan = "5B";
    else if (freq == 178352000) chan = "5C";
    else if (freq == 180064000) chan = "5D";
    else if (freq == 181936000) chan = "6A";
    else if (freq == 183648000) chan = "6B";
    else if (freq == 185360000) chan = "6C";
    else if (freq == 187072000) chan = "6D";
    else if (freq == 188928000) chan = "7A";
    else if (freq == 190640000) chan = "7B";
    else if (freq == 192352000) chan = "7C";
    else if (freq == 194064000) chan = "7D";
    else if (freq == 195936000) chan = "8A";
    else if (freq == 197648000) chan = "8B";
    else if (freq == 199360000) chan = "8C";
    else if (freq == 201072000) chan = "8D";
    else if (freq == 202928000) chan = "9A";
    else if (freq == 204640000) chan = "9B";
    else if (freq == 206352000) chan = "9C";
    else if (freq == 208064000) chan = "9D";
    else if (freq == 209936000) chan = "10A";
    else if (freq == 211648000) chan = "10B";
    else if (freq == 213360000) chan = "10C";
    else if (freq == 215072000) chan = "10D";
    else if (freq == 216928000) chan = "11A";
    else if (freq == 218640000) chan = "11B";
    else if (freq == 220352000) chan = "11C";
    else if (freq == 222064000) chan = "11D";
    else if (freq == 223936000) chan = "12A";
    else if (freq == 225648000) chan = "12B";
    else if (freq == 227360000) chan = "12C";
    else if (freq == 229072000) chan = "12D";
    else if (freq == 230784000) chan = "13A";
    else if (freq == 232496000) chan = "13B";
    else if (freq == 234208000) chan = "13C";
    else if (freq == 235776000) chan = "13D";
    else if (freq == 237488000) chan = "13E";
    else if (freq == 239200000) chan = "13F";
    else { return std::nullopt; }

    return chan;
}

std::chrono::milliseconds transmission_frame_duration(unsigned int dabmode)
{
    using namespace std::chrono;
    switch (dabmode) {
        case 1: return milliseconds(96);
        case 2: return milliseconds(24);
        case 3: return milliseconds(24);
        case 4: return milliseconds(48);
        default:
            throw std::runtime_error("invalid DAB mode");
    }
}


time_t get_clock_realtime_seconds()
{
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) != 0) {
        throw std::runtime_error(std::string("Failed to retrieve CLOCK_REALTIME") + strerror(errno));
    }

    return t.tv_sec;
}
