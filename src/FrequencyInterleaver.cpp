/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "FrequencyInterleaver.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <malloc.h>
#include <complex>

typedef std::complex<float> complexf;


FrequencyInterleaver::FrequencyInterleaver(size_t mode) :
    ModCodec(ModFormat(0), ModFormat(0))
{
    PDEBUG("FrequencyInterleaver::FrequencyInterleaver(%zu) @ %p\n",
            mode, this);

    size_t num;
    size_t alpha = 13;
    size_t beta;
    switch (mode) {
    case 1:
        d_carriers = 1536;
        num = 2048;
        beta = 511;
        break;
    case 2:
        d_carriers = 384;
        num = 512;
        beta = 127;
        break;
    case 3:
        d_carriers = 192;
        num = 256;
        beta = 63;
        break;
    case 0:
    case 4:
        d_carriers = 768;
        num = 1024;
        beta = 255;
        break;
    default:
        PDEBUG("Carriers: %zu\n", (d_carriers >> 1) << 1);
        throw std::runtime_error("FrequencyInterleaver::FrequencyInterleaver "
                "nb of carriers invalid!");
        break;
    }

    d_indexes = (size_t*)memalign(16, d_carriers * sizeof(size_t));
    size_t* index = d_indexes;
    size_t perm = 0;
    PDEBUG("i: %4u, R: %4u\n", 0, 0);
    for (size_t j = 1; j < num; ++j) {
        perm = (alpha * perm + beta) & (num - 1);
        if (perm >= ((num - d_carriers) / 2)
                && perm <= (num - (num - d_carriers) / 2)
                && perm != (num / 2)) {
            PDEBUG("i: %4zu, R: %4zu, d: %4zu, n: %4zu, k: %5zi, index: %zu\n",
                    j, perm, perm, index - d_indexes, perm - num / 2,
                    perm > num / 2
                    ?  perm - (1 + (num / 2))
                    : perm + (d_carriers - (num / 2)));
            *(index++) = perm > num / 2 ?
                perm - (1 + (num / 2)) : perm + (d_carriers - (num / 2));
        } else {
            PDEBUG("i: %4zu, R: %4zu\n", j, perm);
        }
    }
}


FrequencyInterleaver::~FrequencyInterleaver()
{
    PDEBUG("FrequencyInterleaver::~FrequencyInterleaver() @ %p\n", this);

    free(d_indexes);
}


int FrequencyInterleaver::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FrequencyInterleaver::process"
            "(dataIn: %p, sizeIn: %zu, dataOut: %p, sizeOut: %zu)\n",
            dataIn, dataIn->getLength(), dataOut, dataOut->getLength());

    dataOut->setLength(dataIn->getLength());

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);

    if (sizeIn % d_carriers != 0) {
        throw std::runtime_error(
                "FrequencyInterleaver::process input size not valid!");
    }

    for (size_t i = 0; i < sizeIn;) {
//        memset(out, 0, d_carriers * sizeof(complexf));
        for (size_t j = 0; j < d_carriers; i += 4, j += 4) {
            out[d_indexes[j]] = in[i];
            out[d_indexes[j + 1]] = in[i + 1];
            out[d_indexes[j + 2]] = in[i + 2];
            out[d_indexes[j + 3]] = in[i + 3];
        }
        out += d_carriers;
    }

    return 1;
}
