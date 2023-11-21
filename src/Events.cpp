/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>

#include "Events.h"

EventSender events;

EventSender::EventSender() :
    m_zmq_context(1),
    m_socket(m_zmq_context, zmq::socket_type::pub)
{
    int linger = 2000;
    m_socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

EventSender::~EventSender()
{ }

void EventSender::bind(const std::string& bind_endpoint)
{
    try {
        m_socket.bind(bind_endpoint);
        m_socket_valid = true;
    }
    catch (const zmq::error_t& err) {
        fprintf(stderr, "Cannot bind event socket: %s", err.what());
    }
}

void EventSender::send(const std::string& event_name, const json::map_t& detail)
{
    if (not m_socket_valid) {
        return;
    }

    zmq::message_t zmsg1(event_name.data(), event_name.size());
    const auto detail_json = json::map_to_json(detail);
    zmq::message_t zmsg2(detail_json.data(), detail_json.size());

    try {
        m_socket.send(zmsg1, zmq::send_flags::sndmore);
        m_socket.send(zmsg2, zmq::send_flags::none);
    }
    catch (const zmq::error_t& err) {
        fprintf(stderr, "Cannot send event %s: %s", event_name.c_str(), err.what());
    }
}


void LogToEventSender::log(log_level_t level, const std::string& message)
{
    std::string event_name;
    if (level == log_level_t::warn) { event_name = "warn"; }
    else if (level == log_level_t::error) { event_name = "error"; }
    else if (level == log_level_t::alert) { event_name = "alert"; }
    else if (level == log_level_t::emerg) { event_name = "emerg"; }

    if (not event_name.empty()) {
        json::map_t detail;
        detail["message"].v = message;
        events.send(event_name, detail);
    }
}

std::string LogToEventSender::get_name() const
{
    return "EventSender";
}
