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
#include <boost/thread/thread.hpp>
#include "InputReader.h"
#include "PcDebug.h"
#include "Utils.h"

using namespace std;

#define NUM_FRAMES_PER_ZMQ_MESSAGE 4
/* A concatenation of four ETI frames,
 * whose maximal size is 6144.
 *
 * Four frames in one zmq message are sent, so that
 * we do not risk breaking ETI vs. transmission frame
 * phase.
 *
 * The frames are concatenated in buf, and
 * their sizes is given in the buflen array.
 *
 * Most of the time, the buf will not be completely
 * filled
 */
struct zmq_dab_message_t
{
    uint32_t version;
    uint16_t buflen[NUM_FRAMES_PER_ZMQ_MESSAGE];
    uint8_t  buf[NUM_FRAMES_PER_ZMQ_MESSAGE*6144];
};

#define ZMQ_DAB_MESSAGE_T_HEADERSIZE \
    (sizeof(uint32_t) + NUM_FRAMES_PER_ZMQ_MESSAGE*sizeof(uint16_t))

InputZeroMQReader::~InputZeroMQReader()
{
    m_running = false;
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

    m_recv_thread = boost::thread(&InputZeroMQReader::RecvProcess, this);

    return 0;
}

int InputZeroMQReader::GetNextFrame(void* buffer)
{
    if (not m_running) {
        return 0;
    }

    shared_ptr<vector<uint8_t> > incoming;

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

    if (not m_running) {
        throw zmq_input_overflow();
    }


    const size_t framesize = 6144;
    if (incoming->empty()) {
        return 0;
    }
    else if (incoming->size() == framesize) {
        memcpy(buffer, &incoming->front(), framesize);
    }
    else {
        throw logic_error("ZMQ ETI not 6144");
    }

    return framesize;
}

void InputZeroMQReader::PrintInfo() const
{
    fprintf(stderr, "Input ZeroMQ:\n");
    fprintf(stderr, "  Receiving from %s\n\n", m_uri.c_str());
}

void InputZeroMQReader::RecvProcess()
{
    set_thread_name("zmqinput");
    m_running = true;

    size_t queue_size = 0;
    bool buffer_full = false;

    zmq::socket_t subscriber(m_zmqcontext, ZMQ_SUB);
    // zmq sockets are not thread safe. That's why
    // we create it here, and not at object creation.

    bool success = true;

    try {
        subscriber.connect(m_uri.c_str());
    }
    catch (zmq::error_t& err) {
        etiLog.level(error) << "Failed to connect ZeroMQ socket to '" <<
            m_uri << "': '" << err.what() << "'";
        success = false;
    }

    if (success) try {
        // subscribe to all messages
        subscriber.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
    }
    catch (zmq::error_t& err) {
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
            const int zmq_timeout_ms = 100;
            const int num_events = zmq::poll(items, 1, zmq_timeout_ms);
            if (num_events == 0) {
                // timeout is signalled by an empty buffer
                auto buf = make_shared<vector<uint8_t> >();
                m_in_messages.push(buf);
                continue;
            }

            subscriber.recv(&incoming);

            if (m_to_drop) {
                queue_size = m_in_messages.size();
                if (queue_size > 4) {
                    m_in_messages.notify();
                }
                m_to_drop--;
            }
            else if (queue_size < m_max_queued_frames) {
                if (buffer_full) {
                    etiLog.level(info) << "ZeroMQ buffer recovered: " <<
                        queue_size << " elements";
                    buffer_full = false;
                }

                if (incoming.size() < ZMQ_DAB_MESSAGE_T_HEADERSIZE) {
                    throw runtime_error("ZeroMQ packet too small for header");
                }
                else {
                    const zmq_dab_message_t* dab_msg = (zmq_dab_message_t*)incoming.data();

                    if (dab_msg->version != 1) {
                        etiLog.level(error) <<
                            "ZeroMQ wrong packet version " <<
                            dab_msg->version;
                    }

                    int offset = sizeof(dab_msg->version) +
                        NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg->buflen);

                    for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++) {
                        if (dab_msg->buflen[i] > 6144) {
                            stringstream ss;
                            ss << "ZeroMQ buffer " << i <<
                                " has invalid buflen " << dab_msg->buflen[i];
                            throw runtime_error(ss.str());
                        }
                        else {
                            auto buf = make_shared<vector<uint8_t> >(6144, 0x55);

                            const int framesize = dab_msg->buflen[i];

                            if ((ssize_t)incoming.size() < offset + framesize) {
                                throw runtime_error("ZeroMQ packet too small");
                            }

                            memcpy(&buf->front(),
                                    ((uint8_t*)incoming.data()) + offset,
                                    framesize);

                            offset += framesize;

                            queue_size = m_in_messages.push(buf);
                            etiLog.log(trace, "ZMQ,push %zu", queue_size);
                        }
                    }
                }
            }
            else {
                m_in_messages.notify();

                if (!buffer_full) {
                    etiLog.level(warn) << "ZeroMQ buffer overfull !";

                    buffer_full = true;
                    throw runtime_error("ZMQ input full");
                }

                queue_size = m_in_messages.size();

                /* Drop three more incoming ETI frames before
                 * we start accepting them again, to guarantee
                 * that we keep transmission frame vs. ETI frame
                 * phase.
                 */
                m_to_drop = 3;
            }

            if (queue_size < 5) {
                etiLog.level(warn) << "ZeroMQ buffer low: " << queue_size << " elements !";
            }
        }
    }
    catch (zmq::error_t& err) {
        etiLog.level(error) << "ZeroMQ error during receive: '" << err.what() << "'";
    }
    catch (std::exception& err) {
        etiLog.level(error) << "Exception during receive: '" << err.what() << "'";
    }

    etiLog.level(info) << "ZeroMQ input worker terminated";

    subscriber.close();

    m_running = false;
    m_in_messages.notify();
}

#endif

