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
#include "Utils.h"

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

int InputZeroMQReader::Open(const std::string& uri, size_t max_queued_frames)
{
    // The URL might start with zmq+tcp://
    if (uri.substr(0, 4) == "zmq+") {
        uri_ = uri.substr(4);
    }
    else {
        uri_ = uri;
    }

    workerdata_.uri = uri_;
    workerdata_.max_queued_frames = max_queued_frames;
    // launch receiver thread
    worker_.Start(&workerdata_);

    return 0;
}

int InputZeroMQReader::GetNextFrame(void* buffer)
{
    const size_t framesize = 6144;

    if (not worker_.is_running()) {
        return 0;
    }

    std::shared_ptr<std::vector<uint8_t> > incoming;

    /* Do some prebuffering because reads will happen in bursts
     * (4 ETI frames in TM1) and we should make sure that
     * we can serve the data required for a full transmission frame.
     */
    if (in_messages_.size() < 4) {
        const size_t prebuffering = 10;
        etiLog.log(trace, "ZMQ,wait1");
        in_messages_.wait_and_pop(incoming, prebuffering);
    }
    else {
        etiLog.log(trace, "ZMQ,wait2");
        in_messages_.wait_and_pop(incoming);
    }
    etiLog.log(trace, "ZMQ,pop");

    if (not worker_.is_running()) {
        throw zmq_input_overflow();
    }

    memcpy(buffer, &incoming->front(), framesize);

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
    set_thread_name("zmqinput");
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
                    etiLog.level(info) << "ZeroMQ buffer recovered: " << queue_size << " elements";
                    buffer_full = false;
                }

                zmq_dab_message_t* dab_msg = (zmq_dab_message_t*)incoming.data();

                if (dab_msg->version != 1) {
                    etiLog.level(error) << "ZeroMQ wrong packet version " << dab_msg->version;
                }

                int offset = sizeof(dab_msg->version) +
                    NUM_FRAMES_PER_ZMQ_MESSAGE * sizeof(*dab_msg->buflen);

                for (int i = 0; i < NUM_FRAMES_PER_ZMQ_MESSAGE; i++)
                {
                    if (dab_msg->buflen[i] <= 0 ||
                        dab_msg->buflen[i] > 6144)
                    {
                        etiLog.level(error) << "ZeroMQ buffer " << i << " has invalid length " <<
                            dab_msg->buflen[i];
                        // TODO error handling
                    }
                    else {
                        std::shared_ptr<std::vector<uint8_t> > buf =
                            std::make_shared<std::vector<uint8_t> >(6144, 0x55);

                        const int framesize = dab_msg->buflen[i];

                        memcpy(&buf->front(),
                                ((uint8_t*)incoming.data()) + offset,
                                framesize);

                        offset += framesize;

                        queue_size = workerdata->in_messages->push(buf);
                        etiLog.log(trace, "ZMQ,push %zu", queue_size);
                    }
                }
            }
            else {
                workerdata->in_messages->notify();

                if (!buffer_full) {
                    etiLog.level(warn) << "ZeroMQ buffer overfull !";

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
                etiLog.level(warn) << "ZeroMQ buffer low: " << queue_size << " elements !";
            }
        }
    }
    catch (zmq::error_t& err) {
        etiLog.level(error) << "ZeroMQ error in RecvProcess: '" << err.what() << "'";
    }
    catch (std::exception& err) {
    }

    etiLog.level(info) << "ZeroMQ input worker terminated";

    subscriber.close();

    running = false;
    workerdata->in_messages->notify();
}

void InputZeroMQWorker::Start(struct InputZeroMQThreadData* workerdata)
{
    running = true;
    recv_thread = boost::thread(&InputZeroMQWorker::RecvProcess, this, workerdata);
}

void InputZeroMQWorker::Stop()
{
    running = false;
    zmqcontext.close();
    recv_thread.join();
}

#endif

