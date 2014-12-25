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


FormatConverter::FormatConverter(void) :
    ModCodec(ModFormat(sizeof(complexf)),
            ModFormat(sizeof(int8_t))) { }

/* Expect the input samples to be in the range [-255.0, 255.0] */
int FormatConverter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FormatConverter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    size_t sizeIn = dataIn->getLength() / sizeof(float);
    dataOut->setLength(sizeIn * sizeof(int8_t));

    float* in = reinterpret_cast<float*>(dataIn->getData());
    int8_t* out = reinterpret_cast<int8_t*>(dataOut->getData());

#if 0
    // WARNING: Untested Code Ahead
    assert(sizeIn % 16 == 0);
    assert((uintptr_t)in % 16 == 0);
    for(int i = 0; i < sizeIn; i+=16)
    {
        __m128 a1 = _mm_load_ps(in+i+0);
        __m128 a2 = _mm_load_ps(in+i+4);
        __m128 a3 = _mm_load_ps(in+i+8);
        __m128 a4 = _mm_load_ps(in+i+12);
        __m64 b1 = _mm_cvtps_pi8(a1);
        __m64 b2 = _mm_cvtps_pi8(a2);
        __m64 b3 = _mm_cvtps_pi8(a3);
        __m64 b4 = _mm_cvtps_pi8(a4);
        _mm_store_ps(out+i+0, b1);
        _mm_store_ps(out+i+4, b2);
        _mm_store_ps(out+i+8, b3);
        _mm_store_ps(out+i+12, b4);
    }
#else
    // Slow implementation that uses _ftol()
    for (size_t i = 0; i < sizeIn; i++) {
        if (in[i] > 127.0f) {
            out[i] = 127;
        }
        else if (in[i] < -127.0f) {
            out[i] = -127;
        }
        else {
            out[i] = in[i];
        }
    }
#endif

    return 1;
}

const char* FormatConverter::name()
{
    return "FormatConverter";
}

