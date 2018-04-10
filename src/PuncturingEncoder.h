/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#include <vector>
#include <memory>
#include <string>
#include <cstddef>

#include "PuncturingRule.h"
#include "ModPlugin.h"


class PuncturingEncoder : public ModCodec
{
public:
    /* Initialise a puncturer that does not check if the
     * outgoing data requires padding. To be used for the
     * FIC. The size of the output buffer is derived from
     * the puncturing rules only
     */
    PuncturingEncoder(void);

    /* Initialise a puncturer that checks if there is up to
     * one byte padding needed in the output buffer. See
     * EN 300 401 Table 31 in 11.3.1 UEP coding. Up to one
     * byte of padding is added
     */
    PuncturingEncoder(size_t num_cu);

    void append_rule(const PuncturingRule& rule);
    void append_tail_rule(const PuncturingRule& rule);
    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "PuncturingEncoder"; }
    size_t getInputSize() { return d_in_block_size; }
    size_t getOutputSize() { return d_out_block_size; }

private:
    size_t d_num_cu;
    size_t d_in_block_size;
    size_t d_out_block_size;
    std::vector<PuncturingRule> d_rules;

    // We use a unique_ptr because we don't want to depend
    // on boost::optional here
    std::unique_ptr<PuncturingRule> d_tail_rule;

    void adjust_item_size();
};

