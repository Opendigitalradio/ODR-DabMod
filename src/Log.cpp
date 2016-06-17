/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C), 2016, Matthias P. Braendli, matthias.braendli@mpb.li
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

#include <list>
#include <stdarg.h>
#include <chrono>

#include "Log.h"
#include "Utils.h"

using namespace std;

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
    log_message_t m;
    m.level = level;
    m.message = message;

    m_message_queue.push(std::move(m));
}

void Logger::io_process()
{
    set_thread_name("logger");
    while (1) {
        log_message_t m;
        while (m_message_queue.pop(m) == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto message = m.message;

        if (m.level == debug and m.message.empty()) {
            // Special message to stop thread
            break;
        }

        /* Remove a potential trailing newline.
         * It doesn't look good in syslog
         */
        if (message[message.length()-1] == '\n') {
            message.resize(message.length()-1);
        }

        for (auto &backend : backends) {
            backend->log(m.level, message);
        }

        if (m.level != log_level_t::trace) {
            std::lock_guard<std::mutex> guard(m_cerr_mutex);
            std::cerr << levels_as_str[m.level] << " " << message << std::endl;
        }
    }
}

LogLine Logger::level(log_level_t level)
{
    return LogLine(this, level);
}

void LogToFile::log(log_level_t level, std::string message)
{
    if (level != log_level_t::trace) {
        const char* log_level_text[] = {
            "DEBUG", "INFO", "WARN", "ERROR", "ALERT", "EMERG"};

        // fprintf is thread-safe
        fprintf(log_file, SYSLOG_IDENT ": %s: %s\n",
                log_level_text[(size_t)level], message.c_str());
        fflush(log_file);
    }
}

LogTracer::LogTracer(const string& trace_filename)
{
    name = "TRACE";
    etiLog.level(info) << "Setting up TRACE to " << trace_filename;

    m_trace_file = fopen(trace_filename.c_str(), "a");
    if (m_trace_file == NULL) {
        fprintf(stderr, "Cannot open trace file !");
        throw std::runtime_error("Cannot open trace file !");
    }

    auto now = chrono::steady_clock::now().time_since_epoch();
    m_trace_micros_startup =
        chrono::duration_cast<chrono::microseconds>(now).count();

    fprintf(m_trace_file, "0,TRACER,startup at %ld\n", m_trace_micros_startup);
}

void LogTracer::log(log_level_t level, std::string message)
{
    if (level == log_level_t::trace) {
        const auto now = chrono::steady_clock::now().time_since_epoch();
        const auto micros = chrono::duration_cast<chrono::microseconds>(now).count();

        fprintf(m_trace_file, "%ld,%s\n",
                micros - m_trace_micros_startup,
                message.c_str());
    }
}
