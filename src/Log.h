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

#include <stdio.h>
#include <syslog.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <list>
#include <stdexcept>
#include <string>

#include "porting.h"

#define SYSLOG_IDENT "CRC-DABMOD"
#define SYSLOG_FACILITY LOG_LOCAL0

enum log_level_t {debug = 0, info, warn, error, alert, emerg};

/** Abstract class all backends must inherit from */
class LogBackend {
    public:
        virtual void log(log_level_t level, std::string message) = 0;
        virtual std::string get_name() = 0;
};

/** A Logging backend for Syslog */
class LogToSyslog : public LogBackend {
    public:
        LogToSyslog() {
            name = "SYSLOG";
            openlog(SYSLOG_IDENT, LOG_PID, SYSLOG_FACILITY);
        }

        ~LogToSyslog() {
            closelog();
        }

        void log(log_level_t level, std::string message) {

            int syslog_level = LOG_EMERG;
            switch (level) {
                case debug: syslog_level = LOG_DEBUG; break;
                case alert: syslog_level = LOG_ALERT; break;
                case info:  syslog_level = LOG_INFO; break;
                case warn:  syslog_level = LOG_WARNING; break;
                case error: syslog_level = LOG_ERR; break;
                case emerg: syslog_level = LOG_EMERG; break;
            }
 
            syslog(level, SYSLOG_IDENT " %s", message.c_str());
        }

        std::string get_name() { return name; };

    private:
        std::string name;
};

class LogToFile : public LogBackend {
    public:
        LogToFile(std::string filename) {
            name = "FILE";
            
            log_file = fopen(filename.c_str(), "a");
            if (log_file == NULL) {
                fprintf(stderr, "Cannot open log file !");
                throw new std::runtime_error("Cannot open log file !");
            }
        }

        ~LogToFile() {
            if (log_file != NULL) {
                fclose(log_file);
            }
        }

        void log(log_level_t level, std::string message) {

            const char* log_level_text[] = {"DEBUG", "INFO", "WARN", "ERROR", "ALERT", "EMERG"};
            
            // fprintf is thread-safe
            fprintf(log_file, "CRC-DABMOD: %s: %s\n", log_level_text[(size_t)level], message.c_str());
            fflush(log_file);
        }

        std::string get_name() { return name; };

    private:
        std::string name;
        FILE* log_file;
};

class LogLine;

class Logger {
    public:
        Logger();

        void register_backend(LogBackend* backend);

        /* The LogLevel will call this when a line is complete */
        void log(log_level_t level, std::string message);

        /* Return a LogLine for the given level */
        LogLine level(log_level_t level);
        
    private:
        std::list<LogBackend*> backends;
};

// Accumulate a line of logs, using same syntax as stringstream
class LogLine {
    public:
        LogLine(const LogLine& logline);
        LogLine(Logger* logger, log_level_t level) :
            logger_(logger)
        {
            level_ = level;
        }

        template <typename T>
        LogLine& operator<<(T s) {
            os << s;
            return *this;
        }

        ~LogLine()
        {
            logger_->log(level_, os.str());
        }

    private:
        std::ostringstream os;
        log_level_t level_;
        Logger* logger_;
};


#endif
