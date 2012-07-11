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

#include "NullSymbol.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <string.h>

typedef std::complex<float> complexf;


NullSymbol::NullSymbol(size_t nbCarriers) :
    ModCodec(ModFormat(0), ModFormat(nbCarriers * sizeof(complexf))),
    myNbCarriers(nbCarriers)
{
    PDEBUG("NullSymbol::NullSymbol(%zu) @ %p\n", nbCarriers, this);

}


NullSymbol::~NullSymbol()
{
    PDEBUG("NullSymbol::~NullSymbol() @ %p\n", this);

}


int NullSymbol::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("NullSymbol::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);
    
    dataOut->setLength(myNbCarriers * 2 * sizeof(float));
    bzero(dataOut->getData(), dataOut->getLength());

    return dataOut->getLength();
}
