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

#include "ConvEncoder.h"
#include "PcDebug.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdexcept>


const static unsigned char PARITY[] = {
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};


ConvEncoder::ConvEncoder(size_t framesize) :
    ModCodec(ModFormat(framesize), ModFormat((framesize * 4) + 3)),
    d_framesize(framesize)
{
    PDEBUG("ConvEncoder::ConvEncoder(%zu)\n", framesize);

}


ConvEncoder::~ConvEncoder()
{
    PDEBUG("ConvEncoder::~ConvEncoder()\n");

}


int ConvEncoder::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("ConvEncoder::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    size_t in_block_size = d_framesize;
    size_t out_block_size = (d_framesize * 4) + 3;
    size_t in_offset = 0;
    size_t out_offset = 0;
    unsigned short memory = 0;
    unsigned char data;

    if (dataIn->getLength() != in_block_size) {
        PDEBUG("%zu != %zu != 0\n", dataIn->getLength(), in_block_size);
        throw std::runtime_error(
                "ConvEncoder::process input size not valid!\n");
    }
    dataOut->setLength(out_block_size);
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
    unsigned char* out = reinterpret_cast<unsigned char*>(dataOut->getData());

    // While there is enought input and ouput items
    while (dataIn->getLength() - in_offset >= in_block_size &&
            dataOut->getLength() - out_offset >= out_block_size) {
        for (size_t in_count = 0; in_count < in_block_size; ++in_count) {
            data = in[in_offset];
            //PDEBUG("Input: 0x%x\n", data);
            // For next 4 output bytes
            for (unsigned out_count = 0; out_count < 4; ++out_count) {
                out[out_offset] = 0;
                // For each 4-bit output word
                for (unsigned j = 0; j < 2; ++j) {
                    memory >>= 1;
                    memory |= (data >> 7) << 6;
                    data <<= 1;
                    //PDEBUG("Memory: 0x%x\n", memory);
                    unsigned char poly[4] = {
                        memory & 0x5b,
                        memory & 0x79,
                        memory & 0x65,
                        memory & 0x5b
                    };
                    //PDEBUG("Polys: 0x%x, 0x%x, 0x%x, 0x%x\n", poly[0], poly[1], poly[2], poly[3]);
                    // For each poly
                    for (unsigned k = 0; k < 4; ++k) {
                        out[out_offset] <<= 1;
                        out[out_offset] |= PARITY[poly[k]];
                        //PDEBUG("Out bit: %i\n", out[no] >> 7);
                    }
                }
                //PDEBUG("Out: 0x%x\n", out[no]);
                ++out_offset;
            }
            ++in_offset;
        }
        for (unsigned pad_count = 0; pad_count < 3; ++pad_count) {
            out[out_offset] = 0;
            // For each 4-bit output word
            for (unsigned j = 0; j < 2; ++j) {
                memory >>= 1;
                //PDEBUG("Memory: 0x%x\n", memory);
                unsigned char poly[4] = {
                    memory & 0x5b,
                    memory & 0x79,
                    memory & 0x65,
                    memory & 0x5b
                };
                //PDEBUG("Polys: 0x%x, 0x%x, 0x%x, 0x%x\n", poly[0], poly[1], poly[2], poly[3]);
                // For each poly
                for (unsigned k = 0; k < 4; ++k) {
                    out[out_offset] <<= 1;
                    out[out_offset] |= PARITY[poly[k]];
                    //PDEBUG("Out bit: %i\n", out[no] >> 7);
                }
            }
            //PDEBUG("Out: 0x%x\n", out[no]);
            ++out_offset;
        }
    }

    PDEBUG(" Consume: %zu\n", in_offset);
    PDEBUG(" Return: %zu\n", out_offset);
    return out_offset;
}
