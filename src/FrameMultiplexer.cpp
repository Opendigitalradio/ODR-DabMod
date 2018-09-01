/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "FrameMultiplexer.h"
#include "PcDebug.h"

#include <stdio.h>
#include <string>
#include <stdexcept>
#include <complex>
#include <memory>
#include <assert.h>
#include <string.h>

typedef std::complex<float> complexf;

FrameMultiplexer::FrameMultiplexer(
        const EtiSource& etiSource) :
    ModMux(),
    m_etiSource(etiSource)
{
}

// dataIn[0] -> PRBS
// dataIn[1+] -> subchannels
int FrameMultiplexer::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
    assert(dataIn.size() >= 1);
    assert(dataIn[0]->getLength() == 864 * 8);
    dataOut->setLength(dataIn[0]->getLength());

#ifdef TRACE
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
    const auto subchannels = m_etiSource.getSubchannels();
    if (subchannels.size() != dataIn.size() - 1) {
        throw FrameMultiplexerError(
                "FrameMultiplexer detected subchannel size change from " +
                std::to_string(dataIn.size() - 1) + " to " +
                std::to_string(subchannels.size()));
    }
    auto subchannel = subchannels.begin();
    while (in != dataIn.end()) {
        if ((*subchannel)->framesizeCu() * 8 != (*in)->getLength()) {
            throw FrameMultiplexerError(
                    "FrameMultiplexer detected invalid subchannel size! " +
                    std::to_string((*subchannel)->framesizeCu() * 8) + " != " +
                    std::to_string((*in)->getLength()));
        }
        size_t offset = (*subchannel)->startAddress() * 8;
        memcpy(&out[offset], (*in)->getData(), (*in)->getLength());
        ++in;
        ++subchannel;
    }

    return dataOut->getLength();
}

