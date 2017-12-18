/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModPlugin.h"
#include <sys/types.h>
#include <stdint.h>

/* The PrbsGenerator can work as a ModInput generating a Prbs
 * sequence from the given parameters only, or as a ModCodec
 * XORing incoming data with the PRBS
 */
class PrbsGenerator : public ModPlugin
{
private:
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
    // PRBS accumulator
    uint32_t d_accum;
    uint32_t d_accum_init;
    // Initialization size
    size_t d_init;

public:
    PrbsGenerator(size_t framesize, uint32_t polynomial, uint32_t accum = 0,
            size_t init = 0);
    virtual ~PrbsGenerator();

    int process(std::vector<Buffer*> dataIn, std::vector<Buffer*> dataOut);
    const char* name() { return "PrbsGenerator"; }
};

