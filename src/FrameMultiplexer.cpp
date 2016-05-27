/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#include "FrameMultiplexer.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <complex>
#include <memory>
#include <assert.h>
#include <string.h>

typedef std::complex<float> complexf;

FrameMultiplexer::FrameMultiplexer(
        size_t framesize,
        const std::vector<std::shared_ptr<SubchannelSource> >* subchannels) :
    ModMux(ModFormat(framesize), ModFormat(framesize)),
    d_frameSize(framesize),
    mySubchannels(subchannels)
{
    PDEBUG("FrameMultiplexer::FrameMultiplexer(%zu) @ %p\n", framesize, this);

}


FrameMultiplexer::~FrameMultiplexer()
{
    PDEBUG("FrameMultiplexer::~FrameMultiplexer() @ %p\n", this);

}


// dataIn[0] -> PRBS
// dataIn[1+] -> subchannels
int FrameMultiplexer::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
    assert(dataIn.size() >= 1);
    assert(dataIn[0]->getLength() == 864 * 8);
    dataOut->setLength(dataIn[0]->getLength());

#ifdef DEBUG
    fprintf(stderr, "FrameMultiplexer::process(dataIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %p", dataIn[i]);
    }
    fprintf(stderr, ", sizeIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %zu", dataIn[i]->getLength());
    }
    fprintf(stderr, ", dataOut: %p, sizeOut: %zu)\n", dataOut, dataOut->getLength());
#endif

    uint8_t* out = reinterpret_cast<uint8_t*>(dataOut->getData());
    std::vector<Buffer*>::const_iterator in = dataIn.begin();

    // Write PRBS
    memcpy(out, (*in)->getData(), (*in)->getLength());
    ++in;
    // Write subchannel
    if (mySubchannels->size() != dataIn.size() - 1) {
        throw std::out_of_range(
                "FrameMultiplexer detected subchannel size change!");
    }
    std::vector<std::shared_ptr<SubchannelSource> >::const_iterator subchannel =
        mySubchannels->begin();
    while (in != dataIn.end()) {
        if ((*subchannel)->framesizeCu() * 8 != (*in)->getLength()) {
            throw std::out_of_range(
                    "FrameMultiplexer detected invalid subchannel size!");
        }
        size_t offset = (*subchannel)->startAddress() * 8;
        memcpy(&out[offset], (*in)->getData(), (*in)->getLength());
        ++in;
        ++subchannel;
    }

    return dataOut->getLength();
}

