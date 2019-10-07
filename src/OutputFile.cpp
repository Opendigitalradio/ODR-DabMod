/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

#include "OutputFile.h"
#include "PcDebug.h"
#include "Log.h"

#include <string>
#include <chrono>
#include <stdexcept>
#include <cassert>
#include <cmath>

using namespace std;

OutputFile::OutputFile(const std::string& filename, bool show_metadata) :
    ModOutput(), ModMetadata(),
    myShowMetadata(show_metadata),
    myFilename(filename)
{
    PDEBUG("OutputFile::OutputFile(filename: %s) @ %p\n",
            filename.c_str(), this);

    FILE* fd = fopen(filename.c_str(), "w");
    if (fd == nullptr) {
        perror(filename.c_str());
        throw std::runtime_error(
                "OutputFile::OutputFile() unable to open file!");
    }
    myFile.reset(fd);
}

int OutputFile::process(Buffer* dataIn)
{
    PDEBUG("OutputFile::process(%p)\n", dataIn);
    assert(dataIn != nullptr);

    if (fwrite(dataIn->getData(), dataIn->getLength(), 1, myFile.get()) == 0) {
        throw std::runtime_error(
                "OutputFile::process() unable to write to file!");
    }

    return dataIn->getLength();
}

meta_vec_t OutputFile::process_metadata(const meta_vec_t& metadataIn)
{
    if (myShowMetadata) {
        stringstream ss;

        frame_timestamp first_ts;

        for (const auto& md : metadataIn) {
            if (md.ts) {
                // The following code assumes TM I, where we get called every 96ms.
                // Support for other transmission modes skipped because this is mostly
                // debugging code.

                if (md.ts->fp == 0 or md.ts->fp == 4) {
                    first_ts = *md.ts;
                }

                ss << " FCT=" << md.ts->fct <<
                    " FP=" << (int)md.ts->fp;
                if (md.ts->timestamp_valid) {
                    ss << " TS=" << md.ts->timestamp_sec << " + " <<
                        std::fixed
                        << (double)md.ts->timestamp_pps / 163840000.0 << ";";
                }
                else {
                    ss << " TS invalid;";
                }
            }
            else {
                ss << " void, ";
            }
        }

        if (myLastTimestamp.timestamp_valid) {
            if (first_ts.timestamp_valid) {
                uint32_t timestamp = myLastTimestamp.timestamp_pps;
                timestamp += 96 << 14; // Shift 96ms by 14 to Timestamp level 2
                if (timestamp > 0xf9FFff) {
                    timestamp -= 0xfa0000; // Substract 16384000, corresponding to one second
                    myLastTimestamp.timestamp_sec += 1;
                }
                myLastTimestamp.timestamp_pps = timestamp;

                if (myLastTimestamp.timestamp_sec != first_ts.timestamp_sec or
                        myLastTimestamp.timestamp_pps != first_ts.timestamp_pps) {
                    ss << " TS wrong interval; ";
                }
                myLastTimestamp = first_ts;
            }
            else {
                ss << " TS of FP=0 MISSING; ";
                myLastTimestamp.timestamp_valid = false;
            }
        }
        else {
            // Includes invalid and valid cases
            myLastTimestamp = first_ts;
        }

        if (metadataIn.empty()) {
            etiLog.level(debug) << "Output File got no metadata";
        }
        else {
            using namespace std::chrono;
            const auto now = system_clock::now();
            const int64_t ticks_now = duration_cast<milliseconds>(now.time_since_epoch()).count();
            //const int64_t first_ts_ticks = first_ts.timestamp_sec * 1000 + first_ts.timestamp_pps / 16384;
            const int64_t first_ts_ticks = std::llrint(first_ts.get_real_secs() * 1000);

            ss << " DELTA: " << first_ts_ticks - ticks_now << "ms;";

            etiLog.level(debug) << "Output File metadata: " << ss.str();
        }
    }
    return {};
}

