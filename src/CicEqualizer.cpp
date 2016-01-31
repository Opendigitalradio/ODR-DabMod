/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#include "CicEqualizer.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>


CicEqualizer::CicEqualizer(size_t nbCarriers, size_t spacing, int R) :
    ModCodec(ModFormat(nbCarriers * sizeof(complexf)),
            ModFormat(nbCarriers * sizeof(complexf))),
    myNbCarriers(nbCarriers),
    mySpacing(spacing),
    myFilter(nbCarriers)
{
    PDEBUG("CicEqualizer::CicEqualizer(%zu, %zu, %i) @ %p\n",
            nbCarriers, spacing, R, this);

    const int M = 1;
    const int N = 4;
    const float pi = 4.0f * atanf(1.0f);
    for (size_t i = 0; i < nbCarriers; ++i) {
        int k = i < (nbCarriers + 1) / 2
            ? i + ((nbCarriers & 1) ^ 1)
            : i - (int)nbCarriers;
        float angle = pi * k / spacing;
        if (k == 0) {
            myFilter[i] = 1.0f;
        }
        else {
            myFilter[i] = sinf(angle / R) / sinf(angle * M);
            myFilter[i] = fabsf(myFilter[i]) * R * M;
            myFilter[i] = powf(myFilter[i], N);
        }
        PDEBUG("HCic[%zu -> %i] = %f (%f dB) -> angle: %f\n",
                i, k,myFilter[i], 20.0 * log10(myFilter[i]), angle);
    }
}


CicEqualizer::~CicEqualizer()
{
    PDEBUG("CicEqualizer::~CicEqualizer() @ %p\n", this);
}


int CicEqualizer::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("CicEqualizer::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength());

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    if ((sizeIn % myNbCarriers) != 0) {
        PDEBUG("%zu != %zu\n", sizeIn, myNbCarriers);
        throw std::runtime_error(
                "CicEqualizer::process input size not valid!");
    }

    for (size_t i = 0; i < sizeOut; ) {
        for (size_t j = 0; j < myNbCarriers; ++j, ++i) {
            out[i] = in[i] * myFilter[j];
        }
    }

    return sizeOut;
}

