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

#include "PrbsGenerator.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>


PrbsGenerator::PrbsGenerator(size_t framesize, uint32_t polynomial,
        uint32_t accum, size_t init) :
    ModCodec(ModFormat(0), ModFormat(framesize)),
    d_framesize(framesize),
    d_polynomial(polynomial),
    d_accum(accum),
    d_accum_init(accum),
    d_init(init)
{
    PDEBUG("PrbsGenerator::PrbsGenerator(%zu, %u, %u, %zu) @ %p\n",
            framesize, polynomial, accum, init, this);

    gen_prbs_table();
    gen_weight_table();
}


PrbsGenerator::~PrbsGenerator()
{
    PDEBUG("PrbsGenerator::~PrbsGenerator() @ %p\n", this);

}


/*
 * Generate a table of matrix products to update a 32-bit PRBS generator.
 */
void PrbsGenerator::gen_prbs_table()
{
    int i;
    for (i = 0;  i < 4;  ++i) {
        int j;
        for (j = 0;  j < 256;  ++j) {
            uint32_t prbs_accum = ((uint32_t)j << (i * 8));
            int k;
            for (k = 0;  k < 8;  ++k) {
                prbs_accum = (prbs_accum << 1)
                                ^ parity_check(prbs_accum & d_polynomial);
            }
            d_prbs_table[i][j] = (prbs_accum & 0xff);
        }
    }
}


/*
 * Generate the weight table.
 */
void PrbsGenerator::gen_weight_table()
{
    int i;
    for (i = 0;  i < 256;  ++i) {
        unsigned char mask=1U, ones_count = 0U;
        int j;
        for (j = 0;  j < 8;  ++j) {
            ones_count += ((i & mask) != 0U);
            mask = mask << 1;
        }
        d_weight[i] = ones_count;
    }
}


/*
 * Generate a parity check for a 32-bit word.
 */
uint32_t PrbsGenerator::parity_check(uint32_t prbs_accum)
{
    uint32_t mask=1UL, parity=0UL;
    int i;
    for (i = 0;  i < 32;  ++i) {
        parity ^= ((prbs_accum & mask) != 0UL);
        mask <<= 1;
    }
    return parity;
}


/*
 * Update a 32-bit PRBS generator eight bits at a time.
 */
uint32_t PrbsGenerator::update_prbs()
{
    unsigned char acc_lsb = 0;
    int i;
    for (i = 0; i < 4; ++i) {
//        PDEBUG("0x%x = 0x%x ^ 0x%x\n",
//            acc_lsb ^ d_prbs_table [i][(d_accum >> (i * 8)) & 0xff],
//            acc_lsb, d_prbs_table [i][(d_accum >> (i * 8)) & 0xff]);
        acc_lsb ^= d_prbs_table[i][(d_accum >> (i * 8)) & 0xff];
    }
    return (d_accum << 8) ^ ((uint32_t)acc_lsb);
}


int PrbsGenerator::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("PrbsGenerator::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);
    dataOut->setLength(d_framesize);
    unsigned char* out = reinterpret_cast<unsigned char*>(dataOut->getData());
    
    // Initialization
    if (d_accum_init) {
        d_accum = d_accum_init;
    } else {
        d_accum = 0;
        while (d_accum < d_polynomial) {
            d_accum <<= 1;
            d_accum |= 1;
        }
    }
    //PDEBUG("Polynomial: 0x%x\n", d_polynomial);
    //PDEBUG("Init accum: 0x%x\n", d_accum);
    size_t i = 0;
    while (i < d_init) {
        out[i++] = 0xff;
    }

    for (; i < d_framesize; ++i) {
        // Writting data
        d_accum = update_prbs();
        if ((d_accum_init == 0xa9) && (i % 188 == 0)) { // DVB energy dispersal
            out[i] = 0;
        } else {
            out[i] = (unsigned char)(d_accum & 0xff);
        }
        //PDEBUG("accum: 0x%x\n", d_accum);
    }

    if ((dataIn != NULL) && dataIn->getLength()) {
        PDEBUG(" mixing input\n");
        const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
        if (dataIn->getLength() != dataOut->getLength()) {
            PDEBUG("%zu != %zu\n", dataIn->getLength(), dataOut->getLength());
            throw std::runtime_error("PrbsGenerator::process "
                    "input size is not equal to output size!\n");
        }
        for (size_t i = 0; i < dataOut->getLength(); ++i) {
            out[i] ^= in[i];
        }
    }
    
    return dataOut->getLength();
}
