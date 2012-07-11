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

#ifndef FREQUENCY_INTERLEAVER_H
#define FREQUENCY_INTERLEAVER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"

#include <sys/types.h>


class FrequencyInterleaver : public ModCodec
{
public:
    FrequencyInterleaver(size_t mode);
    virtual ~FrequencyInterleaver();
    FrequencyInterleaver(const FrequencyInterleaver&);
    FrequencyInterleaver& operator=(const FrequencyInterleaver&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "FrequencyInterleaver"; }

protected:
    size_t d_carriers;
    size_t d_num;
    size_t* d_indexes;
};

#endif // FREQUENCY_INTERLEAVER_H
