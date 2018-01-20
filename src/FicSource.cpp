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

#include "FicSource.h"
#include "PcDebug.h"
#include "Log.h"
#include "TimestampDecoder.h"

#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>


const std::vector<PuncturingRule>& FicSource::get_rules()
{
    return d_puncturing_rules;
}


FicSource::FicSource(unsigned ficf, unsigned mid) :
    ModInput()
{
//    PDEBUG("FicSource::FicSource(...)\n");
//    PDEBUG("  Start address: %i\n", d_start_address);
//    PDEBUG("  Framesize: %i\n", d_framesize);
//    PDEBUG("  Protection: %i\n", d_protection);

    if (ficf == 0) {
        d_framesize = 0;
        d_buffer.setLength(0);
        return;
    }

    if (mid == 3) {
        d_framesize = 32 * 4;
        d_puncturing_rules.emplace_back(29 * 16, 0xeeeeeeee);
        d_puncturing_rules.emplace_back(3 * 16, 0xeeeeeeec);
    } else {
        d_framesize = 24 * 4;
        d_puncturing_rules.emplace_back(21 * 16, 0xeeeeeeee);
        d_puncturing_rules.emplace_back(3 * 16, 0xeeeeeeec);
    }
    d_buffer.setLength(d_framesize);
}

size_t FicSource::getFramesize()
{
    return d_framesize;
}

void FicSource::loadFicData(const Buffer& fic)
{
    d_buffer = fic;
}

int FicSource::process(Buffer* outputData)
{
    PDEBUG("FicSource::process (outputData: %p, outputSize: %zu)\n",
            outputData, outputData->getLength());

    if (d_buffer.getLength() != d_framesize) {
        throw std::runtime_error(
                "ERROR: FicSource::process.outputSize != d_framesize: " +
                std::to_string(d_buffer.getLength()) + " != " +
                std::to_string(d_framesize));
    }
    *outputData = d_buffer;

    return outputData->getLength();
}

void FicSource::loadTimestamp(const std::shared_ptr<struct frame_timestamp>& ts)
{
    d_ts = ts;
}


meta_vec_t FicSource::process_metadata(const meta_vec_t& metadataIn)
{
    if (not d_ts) {
        return {};
    }

    using namespace std;
    meta_vec_t md_vec;
    flowgraph_metadata meta;
    meta.ts = d_ts;
    md_vec.push_back(meta);
    return md_vec;
}

