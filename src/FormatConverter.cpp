/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include <sys/types.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>

#ifdef __SSE__
#  include <xmmintrin.h>
#endif

FormatConverter::FormatConverter(const std::string& format) :
    ModCodec(),
    m_format(format)
{ }

/* Expect the input samples to be in the correct range for the required format */
int FormatConverter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FormatConverter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    size_t sizeIn = dataIn->getLength() / sizeof(float);
    float* in = reinterpret_cast<float*>(dataIn->getData());

    if (m_format == "s16") {
        dataOut->setLength(sizeIn * sizeof(int16_t));
        int16_t* out = reinterpret_cast<int16_t*>(dataOut->getData());

        for (size_t i = 0; i < sizeIn; i++) {
            out[i] = in[i];
        }
    }
    else if (m_format == "u8") {
        dataOut->setLength(sizeIn * sizeof(int8_t));
        uint8_t* out = reinterpret_cast<uint8_t*>(dataOut->getData());

        for (size_t i = 0; i < sizeIn; i++) {
            out[i] = in[i] + 128;
        }
    }
    else {
        dataOut->setLength(sizeIn * sizeof(int8_t));
        int8_t* out = reinterpret_cast<int8_t*>(dataOut->getData());

        for (size_t i = 0; i < sizeIn; i++) {
            out[i] = in[i];
        }
    }

    return dataOut->getLength();
}

const char* FormatConverter::name()
{
    return "FormatConverter";
}

