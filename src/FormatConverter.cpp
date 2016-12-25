/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

    This flowgraph block converts complexf to signed integer.
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

#include "FormatConverter.h"
#include "PcDebug.h"

#include <malloc.h>
#include <sys/types.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>

#ifdef __SSE__
#  include <xmmintrin.h>
#endif

FormatConverter::FormatConverter(void) :
    ModCodec() { }

/* Expect the input samples to be in the range [-255.0, 255.0] */
int FormatConverter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FormatConverter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    size_t sizeIn = dataIn->getLength() / sizeof(float);
    dataOut->setLength(sizeIn * sizeof(int8_t));

    float* in = reinterpret_cast<float*>(dataIn->getData());

#if 0
    // Disabled because subscripting a __m64 doesn't seem to work
    // on all platforms.

    /*
      _mm_cvtps_pi8 does:
             |<----------- 128 bits ------------>|
      __m128 |   I1   |   Q1   |   I2   |   Q2   | in float
      __m64  |I1Q1I2Q2|00000000|                   in int8_t
     */

    uint32_t* out = reinterpret_cast<uint32_t*>(dataOut->getData());

    assert(sizeIn % 16 == 0);
    assert((uintptr_t)in % 16 == 0);
    for(size_t i = 0, j = 0; i < sizeIn; i+=16, j+=4)
    {
        __m128 a1 = _mm_load_ps(in+i+0);
        __m128 a2 = _mm_load_ps(in+i+4);
        __m128 a3 = _mm_load_ps(in+i+8);
        __m128 a4 = _mm_load_ps(in+i+12);
        __m64 b1 = _mm_cvtps_pi8(a1);
        __m64 b2 = _mm_cvtps_pi8(a2);
        __m64 b3 = _mm_cvtps_pi8(a3);
        __m64 b4 = _mm_cvtps_pi8(a4);
        out[j+0]  = b1[0];
        out[j+1]  = b2[0];
        out[j+2]  = b3[0];
        out[j+3]  = b4[0];
    }
#else
    int8_t* out = reinterpret_cast<int8_t*>(dataOut->getData());

    // Slow implementation that uses _ftol()
    for (size_t i = 0; i < sizeIn; i++) {
        out[i] = in[i];
    }
#endif

    return 1;
}

const char* FormatConverter::name()
{
    return "FormatConverter";
}

