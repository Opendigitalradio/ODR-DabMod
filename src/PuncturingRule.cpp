/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#include "PcDebug.h"
#include "PuncturingRule.h"
#include <stdio.h>

PuncturingRule::PuncturingRule(
        const size_t length,
        const uint32_t pattern) :
    d_length(length),
    d_pattern(pattern)
{ }

size_t PuncturingRule::bit_size() const
{
    size_t bits = 0;
    for (uint32_t mask = 0x80000000; mask != 0; mask >>= 1) {
        if (d_pattern & mask) {
            ++bits;
        }
    }
    return bits;
}
