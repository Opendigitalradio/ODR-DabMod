/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C), 2014, Matthias P. Braendli, matthias.braendli@mpb.li
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
#include <stdarg.h>

#include "Log.h"

Logger etiLog;


void Logger::register_backend(LogBackend* backend) {
    backends.push_back(backend);
    //log(info, "Registered new logger " + backend->get_name());
}


void Logger::log(log_level_t level, const char* fmt, ...)
{
    int size = 100;
    std::string str;
    va_list ap;
    while (1) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt, ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }

    logstr(level, str);
}

void Logger::logstr(log_level_t level, std::string message)
{
    /* Remove a potential trailing newline.
     * It doesn't look good in syslog
     */
    if (message[message.length()-1] == '\n') {
        message.resize(message.length()-1);
    }

    for (std::list<LogBackend*>::iterator it = backends.begin();
            it != backends.end();
            ++it) {
        (*it)->log(level, message);
    }

    std::cerr << levels_as_str[level] << " " << message << std::endl;
}


LogLine Logger::level(log_level_t level)
{
    return LogLine(this, level);
}
