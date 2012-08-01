/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Her
   Majesty the Queen in Right of Canada (Communications Research Center
   Canada)
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

#ifndef PC_DEBUG_H
#define PC_DEBUG_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdio.h>

#define LOG	stderr

#if !defined(_WIN32) || defined(__MINGW32__)
#  ifndef PDEBUG 
#    ifdef DEBUG 
#      define PDEBUG(fmt, args...) fprintf (LOG, fmt , ## args) 
#    else 
#      define PDEBUG(fmt, args...)
#    endif 
#  endif
#  ifdef DEBUG
#    define PDEBUG_VERBOSE(level, verbosity, fmt, args...) if (level <= verbosity)  { fprintf(LOG, fmt, ## args); fflush(LOG); }
#    define PDEBUG0_VERBOSE(level, verbosity, txt) if (level <= verbosity)  { fprintf(LOG, txt); fflush(LOG); }
#    define PDEBUG1_VERBOSE(level, verbosity, txt, arg0) if (level <= verbosity)  { fprintf(LOG, txt, arg0); fflush(LOG); }
#    define PDEBUG2_VERBOSE(level, verbosity, txt, arg0, arg1) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1); fflush(LOG); }
#    define PDEBUG3_VERBOSE(level, verbosity, txt, arg0, arg1, arg2) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1, arg2); fflush(LOG); }
#    define PDEBUG4_VERBOSE(level, verbosity, txt, arg0, arg1, arg2, arg3) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1, arg2, arg3); fflush(LOG); }
#  else
#    define PDEBUG_VERBOSE(level, verbosity, fmt, args...)
#    define PDEBUG0_VERBOSE(level, verbosity, txt)
#    define PDEBUG1_VERBOSE(level, verbosity, txt, arg0)
#    define PDEBUG2_VERBOSE(level, verbosity, txt, arg0, arg1)
#    define PDEBUG3_VERBOSE(level, verbosity, txt, arg0, arg1, arg2)
#    define PDEBUG4_VERBOSE(level, verbosity, txt, arg0, arg1, arg2, arg3)
#  endif // DEBUG
#else  // _WIN32
#  ifdef _DEBUG
#    define PDEBUG
#    define PDEBUG_VERBOSE
#    define PDEBUG0_VERBOSE(level, verbosity, txt) if (level <= verbosity)  { fprintf(LOG, txt); fflush(LOG); }
#    define PDEBUG1_VERBOSE(level, verbosity, txt, arg0) if (level <= verbosity)  { fprintf(LOG, txt, arg0); fflush(LOG); }
#    define PDEBUG2_VERBOSE(level, verbosity, txt, arg0, arg1) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1); fflush(LOG); }
#    define PDEBUG3_VERBOSE(level, verbosity, txt, arg0, arg1, arg2) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1, arg2); fflush(LOG); }
#    define PDEBUG4_VERBOSE(level, verbosity, txt, arg0, arg1, arg2, arg3) if (level <= verbosity)  { fprintf(LOG, txt, arg0, arg1, arg2, arg3); fflush(LOG); }
#  else
#    define PDEBUG
#    define PDEBUG_VERBOSE
#    define PDEBUG0_VERBOSE(level, verbosity, txt)
#    define PDEBUG1_VERBOSE(level, verbosity, txt, arg0)
#    define PDEBUG2_VERBOSE(level, verbosity, txt, arg0, arg1)
#    define PDEBUG3_VERBOSE(level, verbosity, txt, arg0, arg1, arg2)
#    define PDEBUG4_VERBOSE(level, verbosity, txt, arg0, arg1, arg2, arg3)
#  endif
#endif

#endif // PC_DEBUG_H
