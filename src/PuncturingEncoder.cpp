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

#include <string>
#include <stdexcept>
#include <cstdio>
#include <cstdint>
#include <cassert>

#include "PuncturingEncoder.h"
#include "PcDebug.h"

PuncturingEncoder::PuncturingEncoder() :
    ModCodec(),
    d_num_cu(0),
    d_in_block_size(0),
    d_out_block_size(0)
{
    PDEBUG("PuncturingEncoder() @ %p\n", this);
}

PuncturingEncoder::PuncturingEncoder(
        size_t num_cu) :
    ModCodec(),
    d_num_cu(num_cu),
    d_in_block_size(0),
    d_out_block_size(0)
{
    PDEBUG("PuncturingEncoder(%zu) @ %p\n", num_cu, this);
}

void PuncturingEncoder::adjust_item_size()
{
    PDEBUG("PuncturingEncoder::adjust_item_size()\n");
    int in_size = 0;
    int out_size = 0;
    int length = 0;

    for (const auto& rule : d_rules) {
        for (length = rule.length(); length > 0; length -= 4) {
            out_size += rule.bit_size();
            in_size += 4;
        }
    }

    if (d_tail_rule) {
        in_size += d_tail_rule->length();
        out_size += d_tail_rule->bit_size();
    }

    d_in_block_size = in_size;
    d_out_block_size = (out_size + 7) / 8;

    PDEBUG(" Puncturing encoder ratio (out/in): %zu / %zu\n",
            d_out_block_size, d_in_block_size);
}


void PuncturingEncoder::append_rule(const PuncturingRule& rule)
{
    PDEBUG("append_rule(rule(%zu, 0x%x))\n", rule.length(), rule.pattern());
    d_rules.push_back(rule);

    adjust_item_size();
}


void PuncturingEncoder::append_tail_rule(const PuncturingRule& rule)
{
    PDEBUG("append_tail_rule(rule(%zu, 0x%x))\n",
            rule.length(), rule.pattern());

    d_tail_rule.reset(new PuncturingRule(rule));

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
    auto rule_it = d_rules.begin();
    PDEBUG(" in block size: %zu\n", d_in_block_size);
    PDEBUG(" out block size: %zu\n", d_out_block_size);

    if (d_num_cu > 0) {
        if (d_num_cu * 8 == d_out_block_size + 1) {
            /* EN 300 401 Table 31 in 11.3.1 UEP coding specifies
             * that we need one byte of padding
             */
            d_out_block_size = d_num_cu * 8;
        }

        if (d_num_cu * 8 != d_out_block_size) {
            throw std::runtime_error(
                    "PuncturingEncoder encoder initialisation failed. "
                    " CU: " + std::to_string(d_num_cu) +
                    " block_size: " + std::to_string(d_out_block_size));
        }
    }

    dataOut->setLength(d_out_block_size);
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
    unsigned char* out = reinterpret_cast<unsigned char*>(dataOut->getData());

    if (dataIn->getLength() != d_in_block_size) {
        throw std::runtime_error(
                "PuncturingEncoder::process wrong input size");
    }

    if (d_tail_rule) {
        d_in_block_size -= d_tail_rule->length();
    }
    while (in_count < d_in_block_size) {
        for (length = rule_it->length(); length > 0; length -= 4) {
            mask = 0x80000000;
            pattern = rule_it->pattern();
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
        if (++rule_it == d_rules.end()) {
            rule_it = d_rules.begin();
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

    for (size_t i = out_count; i < dataOut->getLength(); ++i) {
        out[out_count++] = 0;
    }

    assert(out_count == dataOut->getLength());
    if (out_count != dataOut->getLength()) {
        throw std::runtime_error("PuncturingEncoder::process output size "
                "does not correspond!");
    }

    return d_out_block_size;
}
