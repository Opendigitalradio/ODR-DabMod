/*
   Copyright (C) 2007, 2008, 2009, 2010 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PORTING_H
#define PORTING_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#ifndef HAVE_BZERO
#   define bzero(s, n) memset(s, 0, n)
#endif

#ifndef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#ifdef __cplusplus
extern "C"
#endif
int gettimeofday(struct timeval* t, void* timezone);
#endif

#ifdef _WIN32
#include <fcntl.h>
// For setting default opening mode with fopen as binary, for all files
// including stdin and stdout
extern unsigned int _CRT_fmode;
#endif

#ifndef HAVE_KILL
#   define kill(a, b) raise(b)
#endif

#endif // PORTING_H
