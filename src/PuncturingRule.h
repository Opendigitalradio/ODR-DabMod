/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef PUNCTURING_RULE_H
#define PUNCTURING_RULE_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include <sys/types.h>
#include <stdint.h>


class PuncturingRule
{
private:
    size_t d_length;
    uint32_t d_pattern;

protected:

public:
    PuncturingRule(
            const size_t length,
            const uint32_t pattern);
    virtual ~PuncturingRule();
//    PuncturingRule(const PuncturingRule& rule);
    PuncturingRule& operator=(const PuncturingRule&);

    size_t length() const { return d_length; }
    size_t bit_size() const;
    const uint32_t pattern() const { return d_pattern; }
};


#endif // PUNCTURING_RULE_H
