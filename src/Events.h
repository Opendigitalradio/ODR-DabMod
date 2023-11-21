/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   This module adds remote-control capability to some of the dabmux/dabmod modules.
 */
/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if defined(HAVE_ZEROMQ)
#  include "zmq.hpp"
#endif

#include <list>
#include <unordered_map>
#include <variant>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

#include "Log.h"
#include "Json.h"

class EventSender {
    public:
        EventSender();
        EventSender(const EventSender& other) = delete;
        const EventSender& operator=(const EventSender& other) = delete;
        EventSender(EventSender&& other) = delete;
        EventSender& operator=(EventSender&& other) = delete;
        ~EventSender();

        void bind(const std::string& bind_endpoint);

        void send(const std::string& event_name, const json::map_t& detail);
    private:
        zmq::context_t m_zmq_context;
        zmq::socket_t m_socket;
        bool m_socket_valid = false;
};

class LogToEventSender: public LogBackend {
    public:
        virtual ~LogToEventSender() {};
        virtual void log(log_level_t level, const std::string& message);
        virtual std::string get_name() const;
};

/* events is a singleton used in all parts of the program to output log messages.
 * It is constructed in Events.cpp */
extern EventSender events;

