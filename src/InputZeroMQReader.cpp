/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyrigth (C) 2013
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#if defined(HAVE_INPUT_ZEROMQ)

#include <string>
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include "zmq.hpp"
#include <boost/thread/thread.hpp>
#include "porting.h"
#include "InputReader.h"
#include "PcDebug.h"

#define MAX_QUEUE_SIZE 50

int InputZeroMQReader::Open(std::string uri)
{
    uri_ = uri;
    workerdata_.uri = uri;
    // launch receiver thread
    worker_.Start(&workerdata_);

    return 0;
}

int InputZeroMQReader::GetNextFrame(void* buffer)
{
    zmq::message_t* incoming;
    in_messages_.wait_and_pop(incoming);

    size_t framesize = incoming->size();

    // guarantee that we never will write more than 6144 bytes
    if (framesize > 6144) {
        fprintf(stderr, "ZeroMQ message too large: %zu!\n", framesize);
        logger_.level(error) << "ZeroMQ message too large" << framesize;
        return -1;
    }

    memcpy(buffer, incoming->data(), framesize);

    delete incoming;

    // pad to 6144 bytes
    memset(&((uint8_t*)buffer)[framesize], 0x55, 6144 - framesize);


    return 6144;
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

            if (queue_size < MAX_QUEUE_SIZE) {
                if (buffer_full) {
                    fprintf(stderr, "ZeroMQ buffer recovered: %zu elements\n",
                            queue_size);
                    buffer_full = false;
                }

                zmq::message_t* holder = new zmq::message_t();
                holder->move(&incoming); // move the message into the holder
                queue_size = workerdata->in_messages->push(holder);
            }
            else
            {
                if (!buffer_full) {
                    workerdata->in_messages->notify();
                    fprintf(stderr, "ZeroMQ buffer overfull !\n");

                    buffer_full = true;
                }
            }

            if (queue_size < 5) {
                fprintf(stderr, "ZeroMQ buffer underfull: %zu elements !\n",
                        queue_size);
            }
        }
    }
    catch (zmq::error_t& err) {
        printf("ZeroMQ error in RecvProcess: '%s'\n", err.what());
    }

    subscriber.close();
}

void InputZeroMQWorker::Start(struct InputZeroMQThreadData* workerdata)
{
    running = true;
    recv_thread = boost::thread(&InputZeroMQWorker::RecvProcess, this, workerdata);
}

void InputZeroMQWorker::Stop()
{
    running = false;
    recv_thread.join();
}

#endif

