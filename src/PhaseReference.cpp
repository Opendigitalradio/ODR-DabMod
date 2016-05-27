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

#include "PhaseReference.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <complex>
#include <string.h>

typedef std::complex<float> complexf;

/* ETSI EN 300 401 Table 43 (Clause 14.3.2)
 * Contains h_{i,k} values
 */
const uint8_t PhaseReference::d_h[4][32] = {
    /* h0 */ { 0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1,
        0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1 },
    /* h1 */ { 0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0,
        0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0 },
    /* h2 */ { 0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3,
        0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3 },
    /* h3 */  { 0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2,
        0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2 }
};

/* EN 300 401, Clause 14.3.2:
 * \phi_k = (\pi / 2) * (h_{i,k-k'} + n
 *
 * where "The indices i, k' and the parameter n are specified as functions of
 * the carrier index k for the four transmission modes in tables 44 to 47."
 *
 * Tables 44 to 47 describe the frequency interleaving done in
 * FrequencyInterleaver.
 */


PhaseReference::PhaseReference(unsigned int dabmode) :
    ModCodec(ModFormat(0), ModFormat(0)),
    d_dabmode(dabmode)
{
    PDEBUG("PhaseReference::PhaseReference(%u) @ %p\n", dabmode, this);

    switch (d_dabmode) {
    case 1:
        d_carriers = 1536;
        d_num = 2048;
        break;
    case 2:
        d_carriers = 384;
        d_num = 512;
        break;
    case 3:
        d_carriers = 192;
        d_num = 256;
        break;
    case 4:
        d_dabmode = 0;
    case 0:
        d_carriers = 768;
        d_num = 1024;
        break;
    default:
        throw std::runtime_error(
                "PhaseReference::PhaseReference DAB mode not valid!");
    }
    d_dataIn.resize(d_carriers);
    fillData();

    myOutputFormat.size(d_carriers * sizeof(complexf));
}


PhaseReference::~PhaseReference()
{
    PDEBUG("PhaseReference::~PhaseReference() @ %p\n", this);
}


complexf convert(uint8_t data) {
    const complexf value[] = {
        complexf(1, 0),
        complexf(0, 1),
        complexf(-1, 0),
        complexf(0, -1),
    };
    return value[data % 4];
}


void PhaseReference::fillData()
{
    size_t index;
    size_t offset;
    size_t k;

    const int table[][48][2] = {
        { // Mode 0/4
            // Positive part
            { 0, 0 }, { 3, 1 }, { 2, 0 }, { 1, 2 }, { 0, 0 }, { 3, 1 },
            { 2, 2 }, { 1, 2 }, { 0, 2 }, { 3, 1 }, { 2, 3 }, { 1, 0 },
            // Negative part
            { 0, 0 }, { 1, 1 }, { 2, 1 }, { 3, 2 }, { 0, 2 }, { 1, 2 },
            { 2, 0 }, { 3, 3 }, { 0, 3 }, { 1, 1 }, { 2, 3 }, { 3, 2 },
        },
        { // Mode 1
            // Positive part
            { 0, 3 }, { 3, 1 }, { 2, 1 }, { 1, 1 }, { 0, 2 }, { 3, 2 },
            { 2, 1 }, { 1, 0 }, { 0, 2 }, { 3, 2 }, { 2, 3 }, { 1, 3 },
            { 0, 0 }, { 3, 2 }, { 2, 1 }, { 1, 3 }, { 0, 3 }, { 3, 3 },
            { 2, 3 }, { 1, 0 }, { 0, 3 }, { 3, 0 }, { 2, 1 }, { 1, 1 },
            // Negative part
            { 0, 1 }, { 1, 2 }, { 2, 0 }, { 3, 1 }, { 0, 3 }, { 1, 2 },
            { 2, 2 }, { 3, 3 }, { 0, 2 }, { 1, 1 }, { 2, 2 }, { 3, 3 },
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 3 }, { 0, 2 }, { 1, 2 },
            { 2, 2 }, { 3, 1 }, { 0, 1 }, { 1, 3 }, { 2, 1 }, { 3, 2 },
        },
        { // Mode 2
            // Positive part
            { 2, 0 }, { 1, 2 }, { 0, 2 }, { 3, 1 }, { 2, 0 }, { 1, 3 },
            // Negative part
            { 0, 2 }, { 1, 3 }, { 2, 2 }, { 3, 2 }, { 0, 1 }, { 1, 2 },
        },
        { // Mode 3
            // Positive part
            { 3, 2 }, { 2, 2 }, { 1, 2 },
            // Negative part
            { 0, 2 }, { 1, 3 }, { 2, 0 },
        },
    };

    if (d_dabmode > 3) {
        throw std::runtime_error(
                "PhaseReference::fillData invalid DAB mode!");
    }

    if (d_dataIn.size() != d_carriers) {
        throw std::runtime_error(
                "PhaseReference::fillData d_dataIn has incorrect size!");
    }

    for (index = 0, offset = 0; index < d_dataIn.size(); ++offset) {
        for (k = 0; k < 32; ++k) {
            d_dataIn[index++] = convert(d_h[table[d_dabmode][offset][0]][k]
                    + table[d_dabmode][offset][1]);
        }
    }
}


int PhaseReference::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("PhaseReference::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    if ((dataIn != NULL) && (dataIn->getLength() != 0)) {
        throw std::runtime_error(
                "PhaseReference::process input size not valid!");
    }

    dataOut->setData(&d_dataIn[0], d_carriers * sizeof(complexf));

    return 1;
}

