/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2015
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
#include <sys/prctl.h>
#include <pthread.h>

void printWelcome()
{
    FILE* out = stderr;

    fprintf(out, "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE,
#if defined(GITVERSION)
            GITVERSION,
#else
            VERSION,
#endif
            __DATE__, __TIME__);
}

void printUsage(char* progName)
{
    FILE* out = stderr;
	printWelcome();
    fprintf(out, "Usage with configuration file:\n");
    fprintf(out, "\t%s [-C] config_file.ini\n\n", progName);

    fprintf(out, "Usage with command line options:\n");
    fprintf(out, "\t%s"
            " input"
            " (-f filename | -u uhddevice -F frequency) "
            " [-G txgain]"
            " [-o offset]"
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
    fprintf(out, "                  Specifying this option has two implications: It enables synchronous transmission,\n"
                 "                  requiring an external REFCLK and PPS signal and frames that do not contain a valid timestamp\n"
                 "                  get muted.\n\n");
    fprintf(out, "-T taps_file:  Enable filtering before the output, using the specified file containing the filter taps.\n");
    fprintf(out, "-a gain:       Apply digital amplitude gain.\n");
    fprintf(out, "-c rate:       Set the DAC clock rate and enable Cic Equalisation.\n");
    fprintf(out, "-g:            Set computation gain mode: "
            "%u FIX, %u MAX, %u VAR\n",
            (unsigned int)GainMode::GAIN_FIX,
            (unsigned int)GainMode::GAIN_MAX,
            (unsigned int)GainMode::GAIN_VAR);
    fprintf(out, "-h:            Print this help.\n");
    fprintf(out, "-l:            Loop file when reach end of file.\n");
    fprintf(out, "-m mode:       Set DAB mode: (0: auto, 1-4: force).\n");
    fprintf(out, "-r rate:       Set output sampling rate (default: 2048000).\n\n");
}


void printVersion(void)
{
    FILE *out = stderr;

	printWelcome();
    fprintf(out,
            "    ODR-DabMod is copyright (C) Her Majesty the Queen in Right of Canada,\n"
            "    2009, 2010, 2011, 2012 Communications Research Centre (CRC),\n"
            "     and\n"
            "    Copyright (C) 2014, 2015 Matthias P. Braendli, matthias.braendli@mpb.li\n"
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
    prctl(PR_SET_NAME,name,0,0,0);
}

