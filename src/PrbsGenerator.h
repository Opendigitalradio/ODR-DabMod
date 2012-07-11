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

#ifndef PRBS_GENERATOR_H
#define PRBS_GENERATOR_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"

#include <sys/types.h>
#include <stdint.h>


class PrbsGenerator : public ModCodec
{
private:

protected:
    uint32_t parity_check(uint32_t prbs_accum);
    void gen_prbs_table();
    uint32_t update_prbs();
    void gen_weight_table();
    
    size_t d_framesize;
    // table of matrix products used to update a 32-bit PRBS generator
    uint32_t d_prbs_table [4][256];
    // table of weights for 8-bit bytes
    unsigned char d_weight[256];
    // PRBS polynomial generator
    uint32_t d_polynomial;
    // PRBS generator polarity mask
    unsigned char d_polarity_mask;
    // PRBS accumulator
    uint32_t d_accum;
    uint32_t d_accum_init;
    // Initialization size
    size_t d_init;

public:
    PrbsGenerator(size_t framesize, uint32_t polynomial, uint32_t accum = 0,
            size_t init = 0);
    virtual ~PrbsGenerator();

    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "PrbsGenerator"; }
};


#endif // PRBS_GENERATOR_H
