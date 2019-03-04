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

#include "TimeInterleaver.h"
#include "PcDebug.h"

#include <vector>
#include <string>
#include <stdint.h>


TimeInterleaver::TimeInterleaver(size_t framesize) :
        ModCodec(),
        d_framesize(framesize)
{
    PDEBUG("TimeInterleaver::TimeInterleaver(%zu) @ %p\n", framesize, this);

    if (framesize & 1) {
        throw std::invalid_argument("framesize must be 16 bits multiple");
    }
    for (int i = 0; i < 16; ++i) {
        d_history.emplace_back(framesize, 0);
    }
}


TimeInterleaver::~TimeInterleaver()
{
    PDEBUG("TimeInterleaver::~TimeInterleaver() @ %p\n", this);
}


int TimeInterleaver::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("TimeInterleaver::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    if (dataIn->getLength() != d_framesize) {
        throw std::invalid_argument("Interleaver buffer input size " +
                std::to_string(dataIn->getLength()) + " expected " +
                std::to_string(d_framesize));
    }

    dataOut->setLength(dataIn->getLength());
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
    unsigned char* out = reinterpret_cast<unsigned char*>(dataOut->getData());

    for (size_t i = 0; i < dataOut->getLength();) {
        d_history.push_front(move(d_history.back()));
        d_history.pop_back();
        for (uint_fast16_t j = 0; j < d_framesize;) {
            d_history[0][j] = in[i];
            out[i]  = d_history[0] [j] & 0x80;
            out[i] |= d_history[8] [j] & 0x40;
            out[i] |= d_history[4] [j] & 0x20;
            out[i] |= d_history[12][j] & 0x10;
            out[i] |= d_history[2] [j] & 0x08;
            out[i] |= d_history[10][j] & 0x04;
            out[i] |= d_history[6] [j] & 0x02;
            out[i] |= d_history[14][j] & 0x01;
            ++i;
            ++j;
            d_history[0][j] = in[i];
            out[i]  = d_history[1] [j] & 0x80;
            out[i] |= d_history[9] [j] & 0x40;
            out[i] |= d_history[5] [j] & 0x20;
            out[i] |= d_history[13][j] & 0x10;
            out[i] |= d_history[3] [j] & 0x08;
            out[i] |= d_history[11][j] & 0x04;
            out[i] |= d_history[7] [j] & 0x02;
            out[i] |= d_history[15][j] & 0x01;
            ++i;
            ++j;
        }
    }

    return dataOut->getLength();
}
