/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyrigth (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li


   Input module for reading the ETI data from file or pipe, or ZeroMQ.

   Supported file formats: RAW, FRAMED, STREAMED
   Supports re-sync to RAW ETI file
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

#include <string>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include "InputReader.h"
#include "PcDebug.h"

int InputFileReader::Open(std::string filename, bool loop)
{
    filename_ = filename;
    loop_ = loop;
    FILE* fd = fopen(filename_.c_str(), "r");
    if (fd == nullptr) {
        etiLog.level(error) << "Unable to open input file!";
        perror(filename_.c_str());
        return -1;
    }
    inputfile_.reset(fd);

    return IdentifyType();
}

int InputFileReader::Rewind()
{
    rewind(inputfile_.get()); // Also clears the EOF flag
    return IdentifyType();
}

int InputFileReader::IdentifyType()
{
    EtiStreamType streamType = EtiStreamType::None;

    struct stat inputFileStat;
    fstat(fileno(inputfile_.get()), &inputFileStat);
    inputfilelength_ = inputFileStat.st_size;

    uint32_t sync;
    uint32_t nbFrames;
    uint16_t frameSize;

    char discard_buffer[6144];

    if (fread(&sync, sizeof(sync), 1, inputfile_.get()) != 1) {
        etiLog.level(error) << "Unable to read sync in input file!";
        perror(filename_.c_str());
        return -1;
    }
    if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
        streamType = EtiStreamType::Raw;
        if (inputfilelength_ > 0) {
            nbframes_ = inputfilelength_ / 6144;
        }
        else {
            nbframes_ = ~0;
        }
        if (fseek(inputfile_.get(), -sizeof(sync), SEEK_CUR) != 0) {
            // if the seek fails, consume the rest of the frame
            if (fread(discard_buffer, 6144 - sizeof(sync), 1, inputfile_.get())
                    != 1) {
                etiLog.level(error) << "Unable to read from input file!";
                perror(filename_.c_str());
                return -1;
            }
        }
        this->streamtype_ = streamType;
        return 0;
    }

    nbFrames = sync;
    if (fread(&frameSize, sizeof(frameSize), 1, inputfile_.get()) != 1) {
        etiLog.level(error) << "Unable to read frame size in input file!";
        perror(filename_.c_str());
        return -1;
    }
    sync >>= 16;
    sync &= 0xffff;
    sync |= ((uint32_t)frameSize) << 16;

    if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
        streamType = EtiStreamType::Streamed;
        frameSize = nbFrames & 0xffff;
        if (inputfilelength_ > 0) {
            nbframes_ = inputfilelength_ / (frameSize + 2);
        }
        else {
            nbframes_ = ~0;
        }
        if (fseek(inputfile_.get(), -6, SEEK_CUR) != 0) {
            // if the seek fails, consume the rest of the frame
            if (fread(discard_buffer, frameSize - 4, 1, inputfile_.get())
                    != 1) {
                etiLog.level(error) << "Unable to read from input file!";
                perror(filename_.c_str());
                return -1;
            }
        }
        this->streamtype_ = streamType;
        return 0;
    }

    if (fread(&sync, sizeof(sync), 1, inputfile_.get()) != 1) {
        etiLog.level(error) << "Unable to read nb frame in input file!";
        perror(filename_.c_str());
        return -1;
    }
    if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
        streamType = EtiStreamType::Framed;
        if (fseek(inputfile_.get(), -6, SEEK_CUR) != 0) {
            // if the seek fails, consume the rest of the frame
            if (fread(discard_buffer, frameSize - 4, 1, inputfile_.get())
                    != 1) {
                etiLog.level(error) << "Unable to read from input file!";
                perror(filename_.c_str());
                return -1;
            }
        }
        this->streamtype_ = streamType;
        nbframes_ = ~0;
        return 0;
    }

    // Search for the sync marker byte by byte
    for (size_t i = 10; i < 6144 + 10; ++i) {
        sync >>= 8;
        sync &= 0xffffff;
        if (fread((uint8_t*)&sync + 3, 1, 1, inputfile_.get()) != 1) {
            etiLog.level(error) << "Unable to read from input file!";
            perror(filename_.c_str());
            return -1;
        }
        if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
            streamType = EtiStreamType::Raw;
            if (inputfilelength_ > 0) {
                nbframes_ = (inputfilelength_ - i) / 6144;
            }
            else {
                nbframes_ = ~0;
            }
            if (fseek(inputfile_.get(), -sizeof(sync), SEEK_CUR) != 0) {
                if (fread(discard_buffer, 6144 - sizeof(sync), 1, inputfile_.get())
                        != 1) {
                    etiLog.level(error) << "Unable to read from input file!";
                    perror(filename_.c_str());
                    return -1;
                }
            }
            this->streamtype_ = streamType;
            return 0;
        }
    }

    etiLog.level(error) << "Bad input file format!";
    return -1;
}

std::string InputFileReader::GetPrintableInfo() const
{
    std::string info = "Input file format: ";
    switch (streamtype_) {
        case EtiStreamType::Raw:
            info += "raw";
            break;
        case EtiStreamType::Streamed:
            info += "streamed";
            break;
        case EtiStreamType::Framed:
            info += "framed";
            break;
        default:
            info += "unknown!";
            break;
    }
    info += ", length: " + std::to_string(inputfilelength_);
    if (~nbframes_ != 0) {
        info += ", nb frames: " + std::to_string(nbframes_);
    }
    else {
        info += ", nb frames: endless";
    }
    return info;
}

int InputFileReader::GetNextFrame(void* buffer)
{
    uint16_t frameSize;

    if (streamtype_ == EtiStreamType::Raw) {
        frameSize = 6144;
    }
    else {
        if (fread(&frameSize, sizeof(frameSize), 1, inputfile_.get()) != 1) {
            etiLog.level(error) << "Reached end of file.";
            if (loop_) {
                if (Rewind() == 0) {
                    if (fread(&frameSize, sizeof(frameSize), 1, inputfile_.get()) != 1) {
                        PDEBUG("Error after rewinding file!\n");
                        etiLog.level(error) << "Error after rewinding file!";
                        return -1;
                    }
                }
                else {
                    PDEBUG("Impossible to rewind file!\n");
                    etiLog.level(error) << "Impossible to rewind file!";
                    return -1;
                }
            }
            else {
                return 0;
            }
        }
    }
    if (frameSize > 6144) { // there might be a better limit
        etiLog.level(error) << "Wrong frame size " << frameSize << " in ETI file!";
        return -1;
    }

    PDEBUG("Frame size: %u\n", frameSize);
    size_t read_bytes = fread(buffer, 1, frameSize, inputfile_.get());
    if (    loop_ &&
            streamtype_ == EtiStreamType::Raw && //implies frameSize == 6144
            read_bytes == 0 && feof(inputfile_.get())) {
        // in case of an EOF from a RAW that we loop, rewind
        // otherwise, we won't tolerate it

        if (Rewind() == 0) {
            read_bytes = fread(buffer, 1, frameSize, inputfile_.get());
        }
        else {
            PDEBUG("Impossible to rewind file!\n");
            etiLog.level(error) << "Impossible to rewind file!";
            return -1;
        }
    }


    if (read_bytes != frameSize) {
        // A short read of a frame (i.e. reading an incomplete frame)
        // is not tolerated. Input files must not contain incomplete frames
        if (read_bytes != 0) {
            etiLog.level(error) <<
                    "Unable to read a complete frame of " << frameSize << " data bytes from input file!";
            return -1;
        }
        else {
            return 0;
        }
    }

    memset(&((uint8_t*)buffer)[frameSize], 0x55, 6144 - frameSize);

    return 6144;
}
