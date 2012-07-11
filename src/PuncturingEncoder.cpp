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

#include "PuncturingEncoder.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <stdint.h>
#include <assert.h>


PuncturingEncoder::PuncturingEncoder() :
    ModCodec(ModFormat(0), ModFormat(0)),
    d_in_block_size(0),
    d_out_block_size(0),
    d_tail_rule(NULL)
{
    PDEBUG("PuncturingEncoder() @ %p\n", this);

}


PuncturingEncoder::~PuncturingEncoder()
{
    PDEBUG("PuncturingEncoder::~PuncturingEncoder() @ %p\n", this);
    for (unsigned i = 0; i < d_rules.size(); ++i) {
        PDEBUG(" Deleting rules @ %p\n", d_rules[i]);
        delete d_rules[i];
    }
    if (d_tail_rule != NULL) {
        PDEBUG(" Deleting rules @ %p\n", d_tail_rule);
        delete d_tail_rule;
    }
}

void PuncturingEncoder::adjust_item_size()
{
    PDEBUG("PuncturingEncoder::adjust_item_size()\n");
    int in_size = 0;
    int out_size = 0;
    int length = 0;
    std::vector<PuncturingRule*>::iterator ptr;
    
    for (ptr = d_rules.begin(); ptr != d_rules.end(); ++ptr) {
        for (length = (*ptr)->length(); length > 0; length -= 4) {
            out_size += (*ptr)->bit_size();
            in_size += 4;
//            PDEBUG("- in: %i, out: %i\n", in_size, out_size);
        }
//        PDEBUG("- in: %i, out: %i\n", in_size, (out_size + 7) / 8);
    }
    if (d_tail_rule != NULL) {
//        PDEBUG("- computing tail rule\n");
        in_size += d_tail_rule->length();
        out_size += d_tail_rule->bit_size();
//        PDEBUG("- in: %i, out: %i\n", in_size, out_size);
    }

    d_in_block_size = in_size;
    d_out_block_size = (out_size + 7) / 8;
    myOutputFormat.size(d_out_block_size);
    
    PDEBUG(" Puncturing encoder ratio (out/in): %zu / %zu\n",
            d_out_block_size, d_in_block_size);
}


void PuncturingEncoder::append_rule(const PuncturingRule& rule)
{
    PDEBUG("append_rule(rule(%zu, 0x%x))\n", rule.length(), rule.pattern());
    d_rules.push_back(new PuncturingRule(rule));

    adjust_item_size();
}


void PuncturingEncoder::append_tail_rule(const PuncturingRule& rule)
{
    PDEBUG("append_tail_rule(rule(%zu, 0x%x))\n", rule.length(), rule.pattern());
    d_tail_rule = new PuncturingRule(rule);

    adjust_item_size();
}


int PuncturingEncoder::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("PuncturingEncoder::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);
    size_t in_count = 0;
    size_t out_count = 0;
    size_t bit_count = 0;
    size_t length;
    unsigned char data;
    uint32_t mask;
    uint32_t pattern;
    std::vector<PuncturingRule*>::iterator ptr = d_rules.begin();
    PDEBUG(" in block size: %zu\n", d_in_block_size);
    PDEBUG(" out block size: %zu\n", d_out_block_size);

    dataOut->setLength(d_out_block_size);
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
    unsigned char* out = reinterpret_cast<unsigned char*>(dataOut->getData());
    
    if (dataIn->getLength() != d_in_block_size) {
        throw std::runtime_error("PuncturingEncoder::process wrong input size");
    }

    if (d_tail_rule) {
        d_in_block_size -= d_tail_rule->length();
    }
    while (in_count < d_in_block_size) {
        for (length = (*ptr)->length(); length > 0; length -= 4) {
            mask = 0x80000000;
            pattern = (*ptr)->pattern();
            for (int i = 0; i < 4; ++i) {
                data = in[in_count++];
                for (int j = 0; j < 8; ++j) {
                    if (pattern & mask) {
                        out[out_count] <<= 1;
                        out[out_count] |= data >> 7;
                        if (++bit_count == 8) {
                            bit_count = 0;
                            ++out_count;
                        }
                    }
                    data <<= 1;
                    mask >>= 1;
                }
            }
        }
        if (++ptr == d_rules.end()) {
            ptr = d_rules.begin();
        }
    }
    if (d_tail_rule) {
        d_in_block_size += d_tail_rule->length();
        mask = 0x800000;
        pattern = d_tail_rule->pattern();
        length = d_tail_rule->length();
        for (size_t i = 0; i < length; ++i) {
            data = in[in_count++];
            for (int j = 0; j < 8; ++j) {
                if (pattern & mask) {
                    out[out_count] <<= 1;
                    out[out_count] |= data >> 7;
                    if (++bit_count == 8) {
                        bit_count = 0;
                        ++out_count;
                    }
                }
                data <<= 1;
                mask >>= 1;
            }
        }
    }
    while (bit_count) {
        out[out_count] <<= 1;
        if (++bit_count == 8) {
            bit_count = 0;
            ++out_count;
        }
    }
    for (size_t i = d_out_block_size; i < dataOut->getLength(); ++i) {
        out[out_count++] = 0;
    }
    assert(out_count == dataOut->getLength());
    if (out_count != dataOut->getLength()) {
        throw std::runtime_error("PuncturingEncoder::process output size "
                "does not correspond!");
    }
    
    return d_out_block_size;
}
