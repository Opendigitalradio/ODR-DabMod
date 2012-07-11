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

#ifndef CONV_ENCODER_H
#define CONV_ENCODER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"

#include <sys/types.h>


class ConvEncoder : public ModCodec
{
private:
    size_t d_framesize;

protected:
    
public:
    ConvEncoder(size_t framesize);
    virtual ~ConvEncoder();
    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "ConvEncoder"; }
};


#endif // CONV_ENCODER_H
