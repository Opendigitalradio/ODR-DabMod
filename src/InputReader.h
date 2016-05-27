/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyrigth (C) 2013, 2015
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

#ifndef INPUTREADER_H
#define INPUTREADER_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <cstdio>
#include <vector>
#include <memory>
#if defined(HAVE_ZEROMQ)
#  include "zmq.hpp"
#  include "ThreadsafeQueue.h"
#endif
#include "porting.h"
#include "Log.h"

/* Known types of input streams. Description taken from the CRC mmbTools forum.

    All numbers are little-endian.

    Framed format is used for file recording. It is the default format. The
    padding can be removed from data. Format:
        uint32_t nbFrames
        for each frame
          uint16_t frameSize
          uint8_t data[frameSize]

    Streamed format is used for streamed applications. As the total number of
    frames is unknown before end of transmission, the corresponding field is
    removed. The padding can be removed from data. Format:
        for each frame
          uint16_t frameSize
          uint8_t data[frameSize]

    Raw format is a bit-by-bit (but byte aligned on sync) recording of a G.703
    data stream. The padding is always present. Format:
        for each frame
          uint8_t data[6144]

    Please note that our raw format can also be referred to as ETI(NI, G.703) or ETI(NI).
*/
enum EtiStreamType {
    ETI_STREAM_TYPE_NONE = 0,
    ETI_STREAM_TYPE_RAW,
    ETI_STREAM_TYPE_STREAMED,
    ETI_STREAM_TYPE_FRAMED,
};

class InputReader
{
    public:
        // Put next frame into buffer. This function will never write more than
        // 6144 bytes into buffer.
        // returns number of bytes written to buffer, 0 on eof, -1 on error
        virtual int GetNextFrame(void* buffer) = 0;

        // Print some information
        virtual void PrintInfo() = 0;
};

class InputFileReader : public InputReader
{
    public:
        InputFileReader() :
            streamtype_(ETI_STREAM_TYPE_NONE),
            inputfile_(NULL) { }

        ~InputFileReader()
        {
            if (inputfile_ != NULL) {
                fprintf(stderr, "\nClosing input file...\n");

                fclose(inputfile_);
            }
        }

        // open file and determine stream type
        // When loop=1, GetNextFrame will never return 0
        int Open(std::string filename, bool loop);

        // Print information about the file opened
        void PrintInfo();

        int GetNextFrame(void* buffer);

        EtiStreamType GetStreamType()
        {
            return streamtype_;
        }

    private:
        InputFileReader(const InputFileReader& other);
        InputFileReader& operator=(const InputFileReader& other);

        int IdentifyType();

        // Rewind the file, and replay anew
        // returns 0 on success, -1 on failure
        int Rewind();

        bool loop_; // if shall we loop the file over and over
        std::string filename_;
        EtiStreamType streamtype_;
        FILE* inputfile_;

        size_t inputfilelength_;
        uint64_t nbframes_; // 64-bit because 32-bit overflow is
                            // after 2**32 * 24ms ~= 3.3 years
};

struct zmq_input_overflow : public std::exception
{
  const char* what () const throw ()
  {
    return "InputZMQ buffer overflow";
  }
};

#if defined(HAVE_ZEROMQ)
/* A ZeroMQ input. See www.zeromq.org for more info */

struct InputZeroMQThreadData
{
    ThreadsafeQueue<std::shared_ptr<std::vector<uint8_t> > > *in_messages;
    std::string uri;
    size_t max_queued_frames;
};

class InputZeroMQWorker
{
    public:
        InputZeroMQWorker() :
            running(false),
            zmqcontext(1),
            m_to_drop(0) { }

        void Start(struct InputZeroMQThreadData* workerdata);
        void Stop();

        bool is_running(void) { return running; }
    private:
        bool running;

        void RecvProcess(struct InputZeroMQThreadData* workerdata);

        zmq::context_t zmqcontext; // is thread-safe
        boost::thread recv_thread;

        /* We must be careful to keep frame phase consistent. If we
         * drop a single ETI frame, we will break the transmission
         * frame vs. ETI frame phase.
         *
         * Here we keep track of how many ETI frames we must drop
         */
        int m_to_drop;
};

class InputZeroMQReader : public InputReader
{
    public:
        InputZeroMQReader()
        {
            workerdata_.in_messages = &in_messages_;
        }

        ~InputZeroMQReader()
        {
            worker_.Stop();
        }

        int Open(const std::string& uri, size_t max_queued_frames);

        int GetNextFrame(void* buffer);

        void PrintInfo();

    private:
        InputZeroMQReader(const InputZeroMQReader& other);
        InputZeroMQReader& operator=(const InputZeroMQReader& other);
        std::string uri_;

        InputZeroMQWorker worker_;
        ThreadsafeQueue<std::shared_ptr<std::vector<uint8_t> > > in_messages_;
        struct InputZeroMQThreadData workerdata_;
};

#endif
#endif

