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

#include "NullSymbol.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <string.h>

typedef std::complex<float> complexf;

NullSymbol::NullSymbol(size_t nbCarriers) :
    ModInput(),
    myNbCarriers(nbCarriers)
{
    PDEBUG("NullSymbol::NullSymbol(%zu) @ %p\n", nbCarriers, this);
}


NullSymbol::~NullSymbol()
{
    PDEBUG("NullSymbol::~NullSymbol() @ %p\n", this);
}


int NullSymbol::process(Buffer* dataOut)
{
    PDEBUG("NullSymbol::process(dataOut: %p)\n", dataOut);

    dataOut->setLength(myNbCarriers * 2 * sizeof(float));
    memset(dataOut->getData(), 0, dataOut->getLength());

    return dataOut->getLength();
}

