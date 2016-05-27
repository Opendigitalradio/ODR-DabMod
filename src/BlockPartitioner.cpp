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

#include "BlockPartitioner.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <string.h>
#include <stdint.h>
#include <assert.h>


BlockPartitioner::BlockPartitioner(unsigned mode, unsigned phase) :
    ModMux(ModFormat(0), ModFormat(0)),
    d_mode(mode)
{
    PDEBUG("BlockPartitioner::BlockPartitioner(%i)\n", mode);

    switch (mode) {
    case 1:
        d_ficSize = 2304 / 8;
        d_cifCount = 4;
        d_outputFramesize = 3072 / 8;
        d_outputFramecount = 72;
        break;
    case 2:
        d_ficSize = 2304 / 8;
        d_cifCount = 1;
        d_outputFramesize = 768 / 8;
        d_outputFramecount = 72;
        break;
    case 3:
        d_ficSize = 3072 / 8;
        d_cifCount = 1;
        d_outputFramesize = 384 / 8;
        d_outputFramecount = 144;
        break;
    case 4:
        d_ficSize = 2304 / 8;
        d_cifCount = 2;
        d_outputFramesize = 1536 / 8;
        d_outputFramecount = 72;
        break;
    default:
        throw std::runtime_error(
                "BlockPartitioner::BlockPartitioner invalid mode");
        break;
    }
    d_cifNb = 0;
    // For Synchronisation purpose, count nb of CIF to drop
    d_cifPhase = phase % d_cifCount;
    d_cifSize = 864 * 8;

    myInputFormat.size(d_cifSize);
    myOutputFormat.size(d_cifSize * d_cifCount);
}


BlockPartitioner::~BlockPartitioner()
{
    PDEBUG("BlockPartitioner::~BlockPartitioner()\n");

}


// dataIn[0] -> FIC
// dataIn[1] -> CIF
int BlockPartitioner::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
    assert(dataIn.size() == 2);
    dataOut->setLength(d_cifCount * (d_ficSize + d_cifSize));

#ifdef DEBUG
    fprintf(stderr, "BlockPartitioner::process(dataIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %p", dataIn[i]);
    }
    fprintf(stderr, ", sizeIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %zu", dataIn[i]->getLength());
    }
    fprintf(stderr, ", dataOut: %p, sizeOut: %zu)\n", dataOut, dataOut->getLength());
#endif

    if (dataIn[0]->getLength() != d_ficSize) {
        fprintf(stderr, "FIC is length %zu, should be %zu\n",
                dataIn[0]->getLength(), d_ficSize);

        throw std::runtime_error(
                "BlockPartitioner::process input 0 size not valid!");
    }
    if (dataIn[1]->getLength() != d_cifSize) {
        throw std::runtime_error(
                "BlockPartitioner::process input 1 size not valid!");
    }

    // Synchronize CIF phase
    if (d_cifPhase != 0) {
        if (++d_cifPhase == d_cifCount) {
            d_cifPhase = 0;
        }
        // Drop CIF
        return 0;
    }

    uint8_t* fic = reinterpret_cast<uint8_t*>(dataIn[0]->getData());
    uint8_t* cif = reinterpret_cast<uint8_t*>(dataIn[1]->getData());
    uint8_t* out = reinterpret_cast<uint8_t*>(dataOut->getData());

    // Copy FIC data
    PDEBUG("Writting FIC %zu bytes to %zu\n", d_ficSize, d_cifNb * d_ficSize);
    memcpy(out + (d_cifNb * d_ficSize), fic, d_ficSize);
    // Copy CIF data
    PDEBUG("Writting CIF %u bytes to %zu\n", 864 * 8,
            (d_cifCount * d_ficSize) + (d_cifNb * 864 * 8));
    memcpy(out + (d_cifCount * d_ficSize) + (d_cifNb * 864 * 8), cif, 864 * 8);

    if (++d_cifNb == d_cifCount) {
        d_cifNb = 0;
    }

    return d_cifNb == 0;
}
