/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)
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

#include "OutputMemory.h"
#include "PcDebug.h"

#include <stdexcept>
#include <string.h>


OutputMemory::OutputMemory(Buffer* dataOut)
    : ModOutput(ModFormat(1), ModFormat(0))
{
    PDEBUG("OutputMemory::OutputMemory(%p) @ %p\n", dataOut, this);

    setOutput(dataOut);
}


OutputMemory::~OutputMemory()
{
    PDEBUG("OutputMemory::~OutputMemory() @ %p\n", this);
}


void OutputMemory::setOutput(Buffer* dataOut)
{
    myDataOut = dataOut;
    myInputFormat.size(dataOut == NULL ? 0 : dataOut->getLength());
}


int OutputMemory::process(Buffer* dataIn, Buffer* dataOut)
{
    PDEBUG("OutputMemory::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    *myDataOut = *dataIn;

    return myDataOut->getLength();
}
