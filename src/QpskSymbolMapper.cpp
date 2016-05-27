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

#include "QpskSymbolMapper.h"
#include "PcDebug.h"

#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <complex>
#include <math.h>
#ifdef __SSE__
#   include <xmmintrin.h>
#endif // __SSE__

typedef std::complex<float> complexf;


QpskSymbolMapper::QpskSymbolMapper(size_t carriers) :
    ModCodec(ModFormat(carriers / 4), ModFormat(carriers * 8)),
    d_carriers(carriers)
{
    PDEBUG("QpskSymbolMapper::QpskSymbolMapper(%zu) @ %p\n", carriers, this);

}


QpskSymbolMapper::~QpskSymbolMapper()
{
    PDEBUG("QpskSymbolMapper::~QpskSymbolMapper() @ %p\n", this);

}


int QpskSymbolMapper::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("QpskSymbolMapper::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength() * 4 * 2 * sizeof(float));   // 4 output complex symbols per input byte
#ifdef __SSE__
    const uint8_t* in = reinterpret_cast<const uint8_t*>(dataIn->getData());
    __m128* out = reinterpret_cast<__m128*>(dataOut->getData());

    if (dataIn->getLength() % (d_carriers / 4) != 0) {
        fprintf(stderr, "%zu (input size) %% (%zu (carriers) / 4) != 0\n",
                dataIn->getLength(), d_carriers);
        throw std::runtime_error(
                "QpskSymbolMapper::process input size not valid!");
    }

    const static __m128 symbols[16] = {
        _mm_setr_ps( M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps( M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,- M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,- M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,- M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
        _mm_setr_ps(-M_SQRT1_2,- M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2)
    };
    size_t inOffset = 0;
    size_t outOffset = 0;
    uint8_t tmp = 0;
    for (size_t i = 0; i < dataIn->getLength(); i += d_carriers / 4) {
        for (size_t j = 0; j < d_carriers / 8; ++j) {
            tmp =  (in[inOffset] & 0xc0) >> 4;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0xc0) >> 6;
            out[outOffset] = symbols[tmp];
            tmp =  (in[inOffset] & 0x30) >> 2;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x30) >> 4;
            out[outOffset + 1] = symbols[tmp];
            tmp =  (in[inOffset] & 0x0c);
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x0c) >> 2;
            out[outOffset + 2] = symbols[tmp];
            tmp =  (in[inOffset] & 0x03) << 2;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x03);
            out[outOffset + 3] = symbols[tmp];
            ++inOffset;
            outOffset += 4;
        }
        inOffset += d_carriers / 8;
    }
#else // !__SSE__
    const uint8_t* in = reinterpret_cast<const uint8_t*>(dataIn->getData());
    float* out = reinterpret_cast<float*>(dataOut->getData());
    if (dataIn->getLength() % (d_carriers / 4) != 0) {
        throw std::runtime_error(
                "QpskSymbolMapper::process input size not valid!");
    }
    if (dataOut->getLength() / sizeof(float) != dataIn->getLength() * 4 * 2) {    // 4 output complex symbols per input byte
        throw std::runtime_error(
                "QpskSymbolMapper::process output size not valid!");
    }

    const static float symbols[16][4] = {
        { M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2},
        { M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2},
        { M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2},
        { M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2},
        { M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
        { M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2},
        { M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
        { M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2},
        {-M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2},
        {-M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2},
        {-M_SQRT1_2,- M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2},
        {-M_SQRT1_2,- M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2},
        {-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
        {-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2},
        {-M_SQRT1_2,- M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
        {-M_SQRT1_2,- M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2}
    };
    size_t inOffset = 0;
    size_t outOffset = 0;
    uint8_t tmp;
    for (size_t i = 0; i < dataIn->getLength(); i += d_carriers / 4) {
        for (size_t j = 0; j < d_carriers / 8; ++j) {
            tmp =  (in[inOffset] & 0xc0) >> 4;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0xc0) >> 6;
            memcpy(&out[outOffset], symbols[tmp], sizeof(float) * 4);
            tmp =  (in[inOffset] & 0x30) >> 2;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x30) >> 4;
            memcpy(&out[outOffset + 4], symbols[tmp], sizeof(float) * 4);
            tmp =  (in[inOffset] & 0x0c);
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x0c) >> 2;
            memcpy(&out[outOffset + 8], symbols[tmp], sizeof(float) * 4);
            tmp =  (in[inOffset] & 0x03) << 2;
            tmp |= (in[inOffset + (d_carriers / 8)] & 0x03);
            memcpy(&out[outOffset + 12], symbols[tmp], sizeof(float) * 4);
            ++inOffset;
            outOffset += 4*4;
        }
        inOffset += d_carriers / 8;
    }
#endif // __SSE__

    return 1;
}

