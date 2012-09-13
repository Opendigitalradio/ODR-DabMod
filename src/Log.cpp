/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Includes modifications for which no copyright is claimed
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
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

#include <list>

#include "Log.h"
#include "porting.h"


Logger::Logger() {
}

void
Logger::register_backend(LogBackend* backend) {
    backends.push_back(backend);
    //log(info, "Registered new logger " + backend->get_name());
}


void
Logger::log(log_level_t level, std::string message) {
    for (std::list<LogBackend*>::iterator it = backends.begin(); it != backends.end(); it++) {
        (*it)->log(level, message);
    }
}


LogLine
Logger::level(log_level_t level)
{
    return LogLine(this, level);
}
