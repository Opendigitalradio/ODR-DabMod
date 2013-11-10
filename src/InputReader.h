/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyrigth (C) 2013
   Matthias P. Braendli, matthias.braendli@mpb.li
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

#ifndef INPUTREADER_H
#define INPUTREADER_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <cstdio>
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
        // Save the next frame into the buffer, and return the number of bytes read.
        virtual int GetNextFrame(void* buffer) = 0;
};

class InputFileReader : public InputReader
{
    public:
        InputFileReader(Logger logger) :
            streamtype_(ETI_STREAM_TYPE_NONE),
            inputfile_(NULL), logger_(logger) {};

        ~InputFileReader()
        {
            fprintf(stderr, "\nClosing input file...\n");

            if (inputfile_ != NULL) {
                fclose(inputfile_);
            }
        }

        // open file and determine stream type
        int Open(std::string filename);

        // Print information about the file opened
        void PrintInfo();

        // Rewind the file, and replay anew
        // returns 0 on success, -1 on failure
        int Rewind();

        // Put next frame into buffer. This function will never write more than
        // 6144 bytes into buffer.
        // returns number of bytes written to buffer, 0 on eof, -1 on error
        int GetNextFrame(void* buffer);

        EtiStreamType GetStreamType()
        {
            return streamtype_;
        }

    private:
        int IdentifyType();

        std::string filename_;
        EtiStreamType streamtype_;
        FILE* inputfile_;
        Logger logger_;

        size_t inputfilelength_;
        uint64_t nbframes_; // 64-bit because 32-bit overflow is
                            // after 2**32 * 24ms ~= 3.3 years
};

#endif
