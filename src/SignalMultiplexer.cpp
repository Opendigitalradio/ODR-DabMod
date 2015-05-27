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

#include "SignalMultiplexer.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <assert.h>
#include <string.h>


SignalMultiplexer::SignalMultiplexer(size_t framesize) :
    ModMux(ModFormat(framesize), ModFormat(framesize)),
    d_frameSize(framesize)
{
    PDEBUG("SignalMultiplexer::SignalMultiplexer(%zu) @ %p\n", framesize, this);

}


SignalMultiplexer::~SignalMultiplexer()
{
    PDEBUG("SignalMultiplexer::~SignalMultiplexer() @ %p\n", this);

}


// dataIn[0] -> null symbol
// dataIn[1] -> MSC symbols
// dataIn[2] -> (optional) TII symbol
int SignalMultiplexer::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
#ifdef DEBUG
    fprintf(stderr, "SignalMultiplexer::process (dataIn:");
    for (unsigned i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %p", dataIn[i]);
    }
    fprintf(stderr, ", sizeIn: ");
    for (unsigned i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %zu", dataIn[i]->getLength());
    }
    fprintf(stderr, ", dataOut: %p, sizeOut: %zu)\n", dataOut, dataOut->getLength());
#endif

    assert(dataIn.size() == 2 or dataIn.size() == 3);

    if (dataIn.size() == 2) {
        *dataOut = *dataIn[0];
        *dataOut += *dataIn[1];
    }
    else if (dataIn.size() == 3) {
        *dataOut = *dataIn[2];
        *dataOut += *dataIn[1];
    }

    return dataOut->getLength();
}

