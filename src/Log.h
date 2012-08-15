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

#ifndef _LOG_H
#define _LOG_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <string>
#include <syslog.h>
#include <iostream>
#include <fstream>
#include <list>
#include <stdexcept>

#include <stdarg.h>

#include "porting.h"

#define SYSLOG_IDENT "CRC-DABMOD"
#define SYSLOG_FACILITY LOG_LOCAL0

enum log_level_t {debug = 0, info, warn, error, alert, emerg};

class LogBackend {
    public:
        virtual void log(log_level_t level, const char* fmt, ...) = 0;
        virtual std::string get_name() = 0;
};

class LogToSyslog : public LogBackend {
    public:
        LogToSyslog() {
            name = "SYSLOG";
            openlog(SYSLOG_IDENT, LOG_PID, SYSLOG_FACILITY);
        }

        ~LogToSyslog() {
            closelog();
        }

        void log(log_level_t level, const char* fmt, ...) {
            va_list arg_ptr;

            int syslog_level = LOG_EMERG;
            switch (level) {
                case debug: syslog_level = LOG_DEBUG; break;
                case alert: syslog_level = LOG_ALERT; break;
                case info:  syslog_level = LOG_INFO; break;
                case warn:  syslog_level = LOG_WARNING; break;
                case error: syslog_level = LOG_ERR; break;
                case emerg: syslog_level = LOG_EMERG; break;
            }
 
            va_start(arg_ptr, fmt);
            syslog(level, fmt, arg_ptr);
            va_end(arg_ptr);
        }

        std::string get_name() { return name; };

    private:
        std::string name;
};

class LogToFile : public LogBackend {
    public:
        LogToFile(std::string filename) {
            name = "FILE";
            log_filename = filename;
            log_stream.open(filename.c_str(), std::ios::app);
            if (!log_stream.is_open()) {
                throw new std::runtime_error("Cannot open log file !");
            }
        }

        ~LogToFile() {
            if (log_stream.is_open()) {
                log_stream.close();
            }
        }

        void log(log_level_t level, const char* fmt, ...) {
            va_list arg_ptr;
            char message[200];

            const char* log_level_text[] = {"DEBUG", "INFO", "WARN", "ERROR", "ALERT", "EMERG"};

            va_start(arg_ptr, fmt);
            snprintf(message, 200, fmt, arg_ptr);
            log_stream << "CRC-DABMOD: " << log_level_text[(size_t)level] << ": " << message << std::endl;
            va_end(arg_ptr);
        }

        std::string get_name() { return name; };

    private:
        std::string name;
        std::string log_filename;
        std::ofstream log_stream;
};

class Logger {
    public:
        Logger();

        void register_backend(LogBackend* backend);

        void log(log_level_t level, std::string message) { operator()(level, "%s", message.c_str()); }
        void log(log_level_t level, const char* fmt, ...);

        void operator()(log_level_t level, const char* fmt, ...);

    private:
        std::list<LogBackend*> backends;
};


#endif
