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

#pragma once

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <chrono>

void printUsage(const char* progName);

void printVersion(void);

void printStartupInfo(void);

// Set SCHED_RR with priority prio (0=lowest)
int set_realtime_prio(int prio);

// Set the name of the thread
void set_thread_name(const char *name);

// Convert a channel like 10A to a frequency
double parseChannel(const std::string& chan);

// dabMode is either 1, 2, 3, 4, corresponding to TM I, TM II, TM III and TM IV.
// throws a runtime_error if dabMode is not one of these values.
std::chrono::milliseconds transmission_frame_duration(unsigned int dabmode);
