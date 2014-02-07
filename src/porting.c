/*
   Copyright (C) 2007, 2008, 2009, 2010 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#include "porting.h"


#ifndef HAVE_GETTIMEOFDAY
#include <sys/timeb.h>
int gettimeofday(struct timeval* t, void* timezone)
{
    struct timeb timebuffer;
    ftime(&timebuffer);
    t->tv_sec=timebuffer.time;
    t->tv_usec=1000*timebuffer.millitm;
    return 0;
}
#endif

#ifdef _WIN32
unsigned int _CRT_fmode = _O_BINARY;
#endif
