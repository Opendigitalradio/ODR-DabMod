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

#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cmath>
#ifdef __SSE__
#   include <xmmintrin.h>
#endif // __SSE__

#include "QpskSymbolMapper.h"
#include "PcDebug.h"

QpskSymbolMapper::QpskSymbolMapper(size_t carriers, bool fixedPoint) :
    ModCodec(),
    m_fixedPoint(fixedPoint),
    m_carriers(carriers) { }

int QpskSymbolMapper::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("QpskSymbolMapper::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    // 4 output complex symbols per input byte

    if (m_fixedPoint) {
        dataOut->setLength(dataIn->getLength() * 4 * sizeof(complexfix));

        using fixed_t = complexfix::value_type;

        const uint8_t* in = reinterpret_cast<const uint8_t*>(dataIn->getData());
        fixed_t* out = reinterpret_cast<fixed_t*>(dataOut->getData());

        if (dataIn->getLength() % (m_carriers / 4) != 0) {
            throw std::runtime_error(
                    "QpskSymbolMapper::process input size not valid!");
        }

        constexpr fixed_t v = static_cast<fixed_t>(M_SQRT1_2);

        const static fixed_t symbols[16][4] = {
            { v,  v,  v,  v},
            { v,  v,  v, -v},
            { v, -v,  v,  v},
            { v, -v,  v, -v},
            { v,  v, -v,  v},
            { v,  v, -v, -v},
            { v, -v, -v,  v},
            { v, -v, -v, -v},
            {-v,  v,  v,  v},
            {-v,  v,  v, -v},
            {-v, -v,  v,  v},
            {-v, -v,  v, -v},
            {-v,  v, -v,  v},
            {-v,  v, -v, -v},
            {-v, -v, -v,  v},
            {-v, -v, -v, -v}
        };
        size_t inOffset = 0;
        size_t outOffset = 0;
        uint8_t tmp;
        for (size_t i = 0; i < dataIn->getLength(); i += m_carriers / 4) {
            for (size_t j = 0; j < m_carriers / 8; ++j) {
                tmp =  (in[inOffset] & 0xc0) >> 4;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0xc0) >> 6;
                memcpy(&out[outOffset], symbols[tmp], sizeof(fixed_t) * 4);
                tmp =  (in[inOffset] & 0x30) >> 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x30) >> 4;
                memcpy(&out[outOffset + 4], symbols[tmp], sizeof(fixed_t) * 4);
                tmp =  (in[inOffset] & 0x0c);
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x0c) >> 2;
                memcpy(&out[outOffset + 8], symbols[tmp], sizeof(fixed_t) * 4);
                tmp =  (in[inOffset] & 0x03) << 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x03);
                memcpy(&out[outOffset + 12], symbols[tmp], sizeof(fixed_t) * 4);
                ++inOffset;
                outOffset += 4*4;
            }
            inOffset += m_carriers / 8;
        }
    }
    else {
        dataOut->setLength(dataIn->getLength() * 4 * sizeof(complexf));
#ifdef __SSE__
        const uint8_t* in = reinterpret_cast<const uint8_t*>(dataIn->getData());
        __m128* out = reinterpret_cast<__m128*>(dataOut->getData());

        if (dataIn->getLength() % (m_carriers / 4) != 0) {
            throw std::runtime_error(
                    "QpskSymbolMapper::process input size not valid: " +
                    std::to_string(dataIn->getLength()) +
                    "(input size) % (" + std::to_string(m_carriers) +
                    " (carriers) / 4) != 0");
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
            _mm_setr_ps(-M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2),
            _mm_setr_ps(-M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2),
            _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
            _mm_setr_ps(-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2),
            _mm_setr_ps(-M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2),
            _mm_setr_ps(-M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2)
        };
        size_t inOffset = 0;
        size_t outOffset = 0;
        uint8_t tmp = 0;
        for (size_t i = 0; i < dataIn->getLength(); i += m_carriers / 4) {
            for (size_t j = 0; j < m_carriers / 8; ++j) {
                tmp =  (in[inOffset] & 0xc0) >> 4;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0xc0) >> 6;
                out[outOffset] = symbols[tmp];
                tmp =  (in[inOffset] & 0x30) >> 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x30) >> 4;
                out[outOffset + 1] = symbols[tmp];
                tmp =  (in[inOffset] & 0x0c);
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x0c) >> 2;
                out[outOffset + 2] = symbols[tmp];
                tmp =  (in[inOffset] & 0x03) << 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x03);
                out[outOffset + 3] = symbols[tmp];
                ++inOffset;
                outOffset += 4;
            }
            inOffset += m_carriers / 8;
        }
#else // !__SSE__
        const uint8_t* in = reinterpret_cast<const uint8_t*>(dataIn->getData());
        float* out = reinterpret_cast<float*>(dataOut->getData());
        if (dataIn->getLength() % (m_carriers / 4) != 0) {
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
            {-M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2,  M_SQRT1_2},
            {-M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2},
            {-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
            {-M_SQRT1_2,  M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2},
            {-M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2,  M_SQRT1_2},
            {-M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2, -M_SQRT1_2}
        };
        size_t inOffset = 0;
        size_t outOffset = 0;
        uint8_t tmp;
        for (size_t i = 0; i < dataIn->getLength(); i += m_carriers / 4) {
            for (size_t j = 0; j < m_carriers / 8; ++j) {
                tmp =  (in[inOffset] & 0xc0) >> 4;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0xc0) >> 6;
                memcpy(&out[outOffset], symbols[tmp], sizeof(float) * 4);
                tmp =  (in[inOffset] & 0x30) >> 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x30) >> 4;
                memcpy(&out[outOffset + 4], symbols[tmp], sizeof(float) * 4);
                tmp =  (in[inOffset] & 0x0c);
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x0c) >> 2;
                memcpy(&out[outOffset + 8], symbols[tmp], sizeof(float) * 4);
                tmp =  (in[inOffset] & 0x03) << 2;
                tmp |= (in[inOffset + (m_carriers / 8)] & 0x03);
                memcpy(&out[outOffset + 12], symbols[tmp], sizeof(float) * 4);
                ++inOffset;
                outOffset += 4*4;
            }
            inOffset += m_carriers / 8;
        }
#endif // __SSE__
    }

    return 1;
}

