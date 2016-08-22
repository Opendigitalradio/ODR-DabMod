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

#ifndef __UTILS_H_
#define __UTILS_H_

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

void printWelcome(void);

void printUsage(char* progName);

void printVersion(void);

inline long timespecdiff_us(struct timespec& oldTime, struct timespec& time)
{
    long tv_sec;
    long tv_nsec;
    if (time.tv_nsec < oldTime.tv_nsec) {
        tv_sec = time.tv_sec - 1 - oldTime.tv_sec;
        tv_nsec = 1000000000L + time.tv_nsec - oldTime.tv_nsec;
    }
    else {
        tv_sec = time.tv_sec - oldTime.tv_sec;
        tv_nsec = time.tv_nsec - oldTime.tv_nsec;
    }

    return tv_sec * 1000 + tv_nsec / 1000;
}

// Set SCHED_RR with priority prio (0=lowest)
int set_realtime_prio(int prio);

void set_thread_name(const char *name);

#endif

