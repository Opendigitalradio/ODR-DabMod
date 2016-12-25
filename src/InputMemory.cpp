/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#include "InputMemory.h"
#include "PcDebug.h"

#include <stdexcept>
#include <string.h>


InputMemory::InputMemory(Buffer* dataIn)
    : ModInput()
{
    PDEBUG("InputMemory::InputMemory(%p) @ %p\n",
            dataIn, this);

    setInput(dataIn);
}


InputMemory::~InputMemory()
{
    PDEBUG("InputMemory::~InputMemory() @ %p\n", this);
}


void InputMemory::setInput(Buffer* dataIn)
{
    myDataIn = dataIn;
}


int InputMemory::process(Buffer* dataOut)
{
    PDEBUG("InputMemory::process (dataOut: %p)\n",
            dataOut);

    *dataOut = *myDataIn;

    return dataOut->getLength();
}
