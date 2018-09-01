/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Her
   Majesty the Queen in Right of Canada (Communications Research Center
   Canada)
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

#ifndef PC_DEBUG_H
#define PC_DEBUG_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdio.h>

#define LOG stderr

#ifndef PDEBUG
#  ifdef TRACE
#    define PDEBUG(fmt, args...) fprintf (LOG, fmt , ## args)
#  else
#    define PDEBUG(fmt, args...)
#  endif
#endif

#endif // PC_DEBUG_H
