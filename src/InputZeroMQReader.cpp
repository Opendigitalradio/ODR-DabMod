/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if defined(HAVE_ZEROMQ)

#include <string>
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include "zmq.hpp"
#include "InputReader.h"
#include "PcDebug.h"
#include "Utils.h"

using namespace std;

constexpr int ZMQ_TIMEOUT_MS = 100;

#define NUM_FRAMES_PER_ZMQ_MESSAGE 4
/* A concatenation of four ETI frames,
 * whose maximal size is 6144.
 *
 * Four frames in one zmq message are sent, so that
 * we do not risk breaking ETI vs. transmission frame
 * phase.
 *
 * The header is followed by the four ETI frames.
 */
struct zmq_msg_header_t
{
    uint32_t version;
    uint16_t buflen[NUM_FRAMES_PER_ZMQ_MESSAGE];
};

#define ZMQ_DAB_MESSAGE_T_HEADERSIZE \
    (sizeof(uint32_t) + NUM_FRAMES_PER_ZMQ_MESSAGE*sizeof(uint16_t))

InputZeroMQReader::InputZeroMQReader() :
    InputReader(),
    RemoteControllable("inputzmq")
{
    RC_ADD_PARAMETER(buffer, "Size of input buffer [us] (read-only)");
}

InputZeroMQReader::~InputZeroMQReader()
{
    m_running = false;
    // This avoids the ugly "context was terminated" error because it lets
    // poll do its thing first
    this_thread::sleep_for(chrono::milliseconds(2 * ZMQ_TIMEOUT_MS));
    m_zmqcontext.close();
    if (m_recv_thread.joinable()) {
        m_recv_thread.join();
    }
}

int InputZeroMQReader::Open(const string& uri, size_t max_queued_frames)
{
    // The URL might start with zmq+tcp://
    if (uri.substr(0, 4) == "zmq+") {
        m_uri = uri.substr(4);
    }
    else {
        m_uri = uri;
    }

    m_max_queued_frames = max_queued_frames;

    m_running = true;
    m_recv_thread = std::thread(&InputZeroMQReader::RecvProcess, this);

    return 0;
}

int InputZeroMQReader::GetNextFrame(void* buffer)
{
    if (not m_running) {
        throw runtime_error("ZMQ input is not ready yet");
    }

    message_t incoming;

    /* Do some prebuffering because reads will happen in bursts
     * (4 ETI frames in TM1) and we should make sure that
     * we can serve the data required for a full transmission frame.
     */
    if (m_in_messages.size() < 4) {
        const size_t prebuffering = 10;
        etiLog.log(trace, "ZMQ,wait1");
        m_in_messages.wait_and_pop(incoming, prebuffering);
    }
    else {
        etiLog.log(trace, "ZMQ,wait2");
        m_in_messages.wait_and_pop(incoming);
    }
    etiLog.log(trace, "ZMQ,pop");

    constexpr size_t framesize = 6144;

    if (incoming.timeout) {
        return 0;
    }
    else if (incoming.fault) {
        throw runtime_error("ZMQ input has terminated");
    }
    else if (incoming.overflow) {
        throw zmq_input_overflow();
    }
    else if (incoming.eti_frame.size() == framesize) {
        unique_lock<mutex> lock(m_last_in_messages_size_mutex);
        m_last_in_messages_size--;
        lock.unlock();

        memcpy(buffer, &incoming.eti_frame.front(), framesize);

        return framesize;
    }
    else {
        throw logic_error("ZMQ ETI not 6144");
    }
}

std::string InputZeroMQReader::GetPrintableInfo() const
{
    return "Input ZeroMQ: Receiving from " + m_uri;
}

void InputZeroMQReader::RecvProcess()
{
    set_thread_name("zmqinput");

    size_t queue_size = 0;

    zmq::socket_t subscriber(m_zmqcontext, ZMQ_SUB);
    // zmq sockets are not thread safe. That's why
    // we create it here, and not at object creation.

    bool success = true;

    try {
        subscriber.connect(m_uri.c_str());
    }
    catch (const zmq::error_t& err) {
        etiLog.level(error) << "Failed to connect ZeroMQ socket to '" <<
            m_uri << "': '" << err.what() << "'";
        success = false;
    }

    if (success) try {
        // subscribe to all messages
        subscriber.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
    }
    catch (const zmq::error_t& err) {
        etiLog.level(error) << "Failed to subscribe ZeroMQ socket to messages: '" <<
            err.what() << "'";
        success = false;
    }

    if (success) try {
        while (m_running) {
            zmq::message_t incoming;
            zmq::pollitem_t items[1];
            items[0].socket = subscriber;
            items[0].events = ZMQ_POLLIN;
            const int num_events = zmq::poll(items, 1, ZMQ_TIMEOUT_MS);
            if (num_events == 0) {
                message_t msg;
                msg.timeout = true;
                m_in_messages.push(move(msg));
                continue;
            }

            subscriber.recv(&incoming);

            if (queue_size < m_max_queued_frames) {
                if (incoming.size() < ZMQ_DAB_MESSAGE_T_HEADERSIZE) {
                    throw runtime_error("ZeroMQ packet too small for header");
                }
                else {
                    zmq_msg_header_t dab_msg;
                    memcpy(&dab_msg, incoming.data(), sizeof(zmq_msg_header_t));

                    if (dab_msg.version != 1) {
                        etiLog.level(error) <<
                            "ZeroMQ wrong packet version " <<
                            dab_msg.version;
                    }

                    int offset = sizeof(dab_msg.version) +
                        NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg.buflen);

                    for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
                        if (dab_msg.buflen[i] > 6144) {
                            stringstream ss;
                            ss << "ZeroMQ buffer " << i <<
                                " has invalid buflen " << dab_msg.buflen[i];
                            throw runtime_error(ss.str());
                        }
                        else {
                            vector<uint8_t> buf(6144, 0x55);

                            const int framesize = dab_msg.buflen[i];

                            if ((ssize_t)incoming.size() < offset + framesize) {
                                throw runtime_error("ZeroMQ packet too small");
                            }

                            memcpy(&buf.front(),
                                    ((uint8_t*)incoming.data()) + offset,
                                    framesize);

                            offset += framesize;

                            message_t msg;
                            msg.eti_frame = move(buf);
                            queue_size = m_in_messages.push(move(msg));
                            etiLog.log(trace, "ZMQ,push %zu", queue_size);

                            unique_lock<mutex> lock(m_last_in_messages_size_mutex);
                            m_last_in_messages_size++;
                        }
                    }
                }
            }
            else {
                message_t msg;
                msg.overflow = true;
                queue_size = m_in_messages.push(move(msg));
                etiLog.level(warn) << "ZeroMQ buffer overfull !";
                throw runtime_error("ZMQ input full");
            }

            if (queue_size < 5) {
                etiLog.level(warn) << "ZeroMQ buffer low: " << queue_size << " elements !";
            }
        }
    }
    catch (const zmq::error_t& err) {
        etiLog.level(error) << "ZeroMQ error during receive: '" << err.what() << "'";
    }
    catch (const std::exception& err) {
        etiLog.level(error) << "Exception during receive: '" << err.what() << "'";
    }

    m_running = false;

    etiLog.level(info) << "ZeroMQ input worker terminated";

    subscriber.close();

    message_t msg;
    msg.fault = true;
    queue_size = m_in_messages.push(move(msg));
}

// =======================================
// Remote Control
// =======================================
void InputZeroMQReader::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "buffer") {
        throw ParameterError("Parameter " + parameter + " is read-only.");
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
    }
}

const string InputZeroMQReader::get_parameter(const string& parameter) const
{
    stringstream ss;
    ss << std::fixed;
    if (parameter == "buffer") {
        // Do not use size of the queue, as it will contain empty
        // frames to signal timeouts
        unique_lock<mutex> lock(m_last_in_messages_size_mutex);
        const long time_in_buffer_us = 24000 * m_last_in_messages_size;
        ss << time_in_buffer_us;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

#endif

