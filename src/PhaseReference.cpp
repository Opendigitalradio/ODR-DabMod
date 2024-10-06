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

#include "PhaseReference.h"
#include "PcDebug.h"

#include <stdexcept>

/* ETSI EN 300 401 Table 43 (Clause 14.3.2)
 * Contains h_{i,k} values
 */
static const uint8_t d_h[4][32] = {
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
PhaseReference::PhaseReference(unsigned int dabmode, bool fixedPoint) :
    ModInput(),
    d_dabmode(dabmode),
    d_fixedPoint(fixedPoint)
{
    PDEBUG("PhaseReference::PhaseReference(%u) @ %p\n", dabmode, this);

    switch (d_dabmode) {
        case 1:
            d_carriers = 1536;
            break;
        case 2:
            d_carriers = 384;
            break;
        case 3:
            d_carriers = 192;
            break;
        case 4:
            d_dabmode = 0;
        case 0:
            d_carriers = 768;
            break;
        default:
            throw std::runtime_error(
                    "PhaseReference::PhaseReference DAB mode not valid!");
    }

    if (d_fixedPoint) {
        d_phaseRefFixed.fillData(d_dabmode, d_carriers);
    }
    else {
        d_phaseRefCF32.fillData(d_dabmode, d_carriers);
    }
}


static const int table[][48][2] = {
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


template <>
complexf PhaseRefGen<complexf>::convert(uint8_t data) {
    const complexf value[] = {
        complexf(1, 0),
        complexf(0, 1),
        complexf(-1, 0),
        complexf(0, -1),
    };
    return value[data % 4];
}

template <>
complexfix PhaseRefGen<complexfix>::convert(uint8_t data) {
    constexpr auto one = fpm::fixed_16_16{1};
    constexpr auto zero = fpm::fixed_16_16{0};

    const complexfix value[] = {
        complexfix(one, zero),
        complexfix(zero, one),
        complexfix(-one, zero),
        complexfix(zero, -one),
    };
    return value[data % 4];
}

template <typename T>
void PhaseRefGen<T>::fillData(unsigned int dabmode, size_t carriers)
{
    dataIn.resize(carriers);
    if (dataIn.size() != carriers) {
        throw std::runtime_error(
                "PhaseReference::fillData dataIn has incorrect size!");
    }

    for (size_t index = 0,
                offset = 0;
                index < dataIn.size();
                ++offset) {
        for (size_t k = 0; k < 32; ++k) {
            dataIn[index++] = convert(
                    d_h[ table[dabmode][offset][0] ][k] +
                    table[dabmode][offset][1] );
        }
    }
}


int PhaseReference::process(Buffer* dataOut)
{
    PDEBUG("PhaseReference::process(dataOut: %p)\n", dataOut);

    if (d_fixedPoint) {
        dataOut->setData(&d_phaseRefFixed.dataIn[0], d_carriers * sizeof(complexfix));
    }
    else {
        dataOut->setData(&d_phaseRefCF32.dataIn[0], d_carriers * sizeof(complexf));
    }

    return 1;
}

