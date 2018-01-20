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
#include "TimestampDecoder.h"

#include <string>
#include <assert.h>
#include <stdexcept>


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

        for (const auto& md : metadataIn) {
            if (md.ts) {
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

        if (metadataIn.empty()) {
            etiLog.level(debug) << "Output File got no mdIn";
        }
        else {
            etiLog.level(debug) << "Output File got metadata: " << ss.str();
        }

    }
    return {};
}

