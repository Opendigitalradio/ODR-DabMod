/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2013, 2014, 2015
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
#include "porting.h"
#include "InputReader.h"
#include "PcDebug.h"

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

int InputZeroMQReader::Open(const std::string& uri, unsigned max_queued_frames)
{
    // The URL might start with zmq+tcp://
    if (uri.substr(0, 4) == "zmq+") {
        uri_ = uri.substr(4);
    }
    else {
        uri_ = uri;
    }

    workerdata_.uri = uri;
    workerdata_.max_queued_frames = max_queued_frames;
    // launch receiver thread
    worker_.Start(&workerdata_);

    return 0;
}

int InputZeroMQReader::GetNextFrame(void* buffer)
{
    const size_t framesize = 6144;

    uint8_t* incoming;
    in_messages_.wait_and_pop(incoming);

    if (! workerdata_.running) {
        throw zmq_input_overflow();
    }

    memcpy(buffer, incoming, framesize);

    delete[] incoming;

    return framesize;
}

void InputZeroMQReader::PrintInfo()
{
    fprintf(stderr, "Input ZeroMQ:\n");
    fprintf(stderr, "  Receiving from %s\n\n", uri_.c_str());
}

// ------------- Worker functions

void InputZeroMQWorker::RecvProcess(struct InputZeroMQThreadData* workerdata)
{
    size_t queue_size = 0;

    bool buffer_full = false;

    zmq::socket_t subscriber(zmqcontext, ZMQ_SUB);
    // zmq sockets are not thread safe. That's why
    // we create it here, and not at object creation.

    try {
        subscriber.connect(workerdata->uri.c_str());

        subscriber.setsockopt(ZMQ_SUBSCRIBE, NULL, 0); // subscribe to all messages

        while (running)
        {
            zmq::message_t incoming;
            subscriber.recv(&incoming);

            if (m_to_drop) {
                queue_size = workerdata->in_messages->size();
                if (queue_size > 4) {
                    workerdata->in_messages->notify();
                }
                m_to_drop--;
            }
            else if (queue_size < workerdata->max_queued_frames) {
                if (buffer_full) {
                    fprintf(stderr, "ZeroMQ buffer recovered: %zu elements\n",
                            queue_size);
                    buffer_full = false;
                }

                zmq_dab_message_t* dab_msg = (zmq_dab_message_t*)incoming.data();

                if (dab_msg->version != 1) {
                    fprintf(stderr, "ZeroMQ input: wrong packet version %d\n",
                            dab_msg->version);
                }

                int offset = sizeof(dab_msg->version) +
                    NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg->buflen);

                for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++)
                {
                    if (dab_msg->buflen[i] <= 0 ||
                        dab_msg->buflen[i] > 6144)
                    {
                        fprintf(stderr, "ZeroMQ buffer %d: invalid length %d\n",
                                i, dab_msg->buflen[i]);
                        // TODO error handling
                    }
                    else {
                        uint8_t* buf = new uint8_t[6144];

                        const int framesize = dab_msg->buflen[i];

                        memcpy(buf,
                                ((uint8_t*)incoming.data()) + offset,
                                framesize);

                        // pad to 6144 bytes
                        memset(&((uint8_t*)buf)[framesize],
                                0x55, 6144 - framesize);

                        offset += framesize;

                        queue_size = workerdata->in_messages->push(buf);
                    }
                }
            }
            else {
                workerdata->in_messages->notify();

                if (!buffer_full) {
                    fprintf(stderr, "ZeroMQ buffer overfull !\n");

                    buffer_full = true;
                    throw std::runtime_error("ZMQ input full");
                }

                queue_size = workerdata->in_messages->size();

                /* Drop three more incoming ETI frames before
                 * we start accepting them again, to guarantee
                 * that we keep transmission frame vs. ETI frame
                 * phase.
                 */
                m_to_drop = 3;
            }

            if (queue_size < 5) {
                fprintf(stderr, "ZeroMQ buffer low: %zu elements !\n",
                        queue_size);
            }
        }
    }
    catch (zmq::error_t& err) {
        fprintf(stderr, "ZeroMQ error in RecvProcess: '%s'\n", err.what());
    }
    catch (std::exception& err) {
    }

    fprintf(stderr, "ZeroMQ input worker terminated\n");

    subscriber.close();

    workerdata->running = false;
    workerdata->in_messages->notify();
}

void InputZeroMQWorker::Start(struct InputZeroMQThreadData* workerdata)
{
    running = true;
    workerdata->running = true;
    recv_thread = boost::thread(&InputZeroMQWorker::RecvProcess, this, workerdata);
}

void InputZeroMQWorker::Stop()
{
    running = false;
    zmqcontext.close();
    recv_thread.join();
}

#endif

