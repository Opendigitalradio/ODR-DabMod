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

#include "Utils.h"
#include "GainControl.h"
#if defined(HAVE_PRCTL)
#  include <sys/prctl.h>
#endif
#include <pthread.h>

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

#if defined(BUILD_FOR_EASYDABV3)
    std::cerr << " This is a build for the EasyDABv3 board" << std::endl;
#endif
}

void printUsage(const char* progName)
{
    FILE* out = stderr;
    fprintf(out, "Usage with configuration file:\n");
    fprintf(out, "\t%s config_file.ini\n\n", progName);

    fprintf(out, "Usage with command line options:\n");
    fprintf(out, "\t%s"
            " input"
#if defined(BUILD_FOR_EASYDABV3)
            " -f filename -F format"
#else
            " (-f filename -F format | -u uhddevice -F frequency)"
#endif
            " [-o offset]"
#if !defined(BUILD_FOR_EASYDABV3)
            "\n\t"
            " [-G txgain]"
            " [-T filter_taps_file]"
            " [-a gain]"
            " [-c clockrate]"
            "\n\t"
            " [-g gainMode]"
            " [-m dabMode]"
            " [-r samplingRate]"
#endif
            " [-l]"
            " [-h]"
            "\n", progName);
    fprintf(out, "Where:\n");
    fprintf(out, "input:         ETI input filename (default: stdin), or\n");
    fprintf(out, "                  tcp://source:port for ETI-over-TCP input, or\n");
    fprintf(out, "                  zmq+tcp://source:port for ZMQ input.\n");
    fprintf(out, "                  udp://:port for EDI input.\n");
    fprintf(out, "-f name:       Use file output with given filename. (use /dev/stdout for standard output)\n");
    fprintf(out, "-F format:     Set the output format (see doc/example.ini for formats) for the file output.\n");
    fprintf(out, "-o:            Set the timestamp offset added to the timestamp in the ETI. The offset is a double.\n");
    fprintf(out, "                  Specifying this option has two implications: It enables synchronous transmission,\n"
                 "                  requiring an external REFCLK and PPS signal and frames that do not contain a valid timestamp\n"
                 "                  get muted.\n\n");
#if !defined(BUILD_FOR_EASYDABV3)
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
#endif
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
            "    Copyright (C) 2018 Matthias P. Braendli, matthias.braendli@mpb.li\n"
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

double parseChannel(const std::string& chan)
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
        std::cerr << "       soapy output: channel " << chan << " does not exist in table\n";
        throw std::out_of_range("soapy channel selection error");
    }
    return freq;
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

