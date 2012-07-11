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

#ifndef PUNCTURING_ENCODER_H
#define PUNCTURING_ENCODER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "PuncturingRule.h"
#include "ModCodec.h"

#include <vector>
#include <sys/types.h>


class PuncturingEncoder : public ModCodec
{
private:
    size_t d_in_block_size;
    size_t d_out_block_size;
    std::vector<PuncturingRule*> d_rules;
    PuncturingRule* d_tail_rule;

protected:
    void adjust_item_size();

public:
    PuncturingEncoder();
    virtual ~PuncturingEncoder();
    PuncturingEncoder(const PuncturingEncoder&);
    PuncturingEncoder& operator=(const PuncturingEncoder&);
    
    void append_rule(const PuncturingRule& rule);
    void append_tail_rule(const PuncturingRule& rule);
    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "PuncturingEncoder"; }
    size_t getInputSize() { return d_in_block_size; }
    size_t getOutputSize() { return d_out_block_size; }
};


#endif // PUNCTURING_ENCODER_H
