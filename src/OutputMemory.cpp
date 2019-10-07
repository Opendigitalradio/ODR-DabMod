/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

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

#include "OutputMemory.h"
#include "PcDebug.h"
#include "Log.h"
#include "TimestampDecoder.h"

#include <stdexcept>
#include <string.h>
#include <math.h>


OutputMemory::OutputMemory(Buffer* dataOut)
    : ModOutput()
{
    PDEBUG("OutputMemory::OutputMemory(%p) @ %p\n", dataOut, this);

    setOutput(dataOut);

#if OUTPUT_MEM_HISTOGRAM
    myMax = 0.0f;
    for (int i = 0; i < HIST_BINS; i++) {
        myHistogram[i] = 0.0f;
    }
#endif
}


OutputMemory::~OutputMemory()
{
#if OUTPUT_MEM_HISTOGRAM
    fprintf(stderr, "* OutputMemory max %f\n", myMax);
    fprintf(stderr, "* HISTOGRAM\n");

    for (int i = 0; i < HIST_BINS; i++) {
        fprintf(stderr, "** %5d - %5d: %ld\n",
                i * HIST_BIN_SIZE,
                (i+1) * HIST_BIN_SIZE - 1,
                myHistogram[i]);
    }
#endif
    PDEBUG("OutputMemory::~OutputMemory() @ %p\n", this);
}


void OutputMemory::setOutput(Buffer* dataOut)
{
    myDataOut = dataOut;
}


int OutputMemory::process(Buffer* dataIn)
{
    PDEBUG("OutputMemory::process(dataIn: %p)\n",
            dataIn);

    *myDataOut = *dataIn;

#if OUTPUT_MEM_HISTOGRAM
    const float* in = (const float*)dataIn->getData();
    const size_t len = dataIn->getLength() / sizeof(float);

    for (size_t i = 0; i < len; i++) {
        float absval = fabsf(in[i]);
        if (myMax < absval)
            myMax = absval;

        myHistogram[lrintf(absval) / HIST_BIN_SIZE]++;
    }
#endif

    return myDataOut->getLength();
}

meta_vec_t OutputMemory::process_metadata(const meta_vec_t& metadataIn)
{
    myMetadata = metadataIn;
    return {};
}

meta_vec_t OutputMemory::get_latest_metadata()
{
    return myMetadata;
}

