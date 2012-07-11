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

#include "ModFormat.h"
#include "PcDebug.h"


ModFormat::ModFormat() : mySize(0)
{
    PDEBUG("ModFormat::ModFormat() @ %p\n", this);
    
}


ModFormat::ModFormat(size_t size) : mySize(size)
{
    PDEBUG("ModFormat::ModFormat(%zu) @ %p\n", size, this);
    
}


ModFormat::~ModFormat()
{
//    PDEBUG("ModFormat::~ModFormat()\n");
//    PDEBUG(" size: %zu\n", mySize);

}


ModFormat::ModFormat(const ModFormat& copy)
{
//    PDEBUG("ModFormat::ModFormat(copy)\n");
//    PDEBUG(" size: %zu\n", copy.mySize);

    mySize = copy.mySize;
}


size_t ModFormat::size()
{
    return mySize;
}


void ModFormat::size(size_t size)
{
    PDEBUG("ModFormat::size(%zu)\n", size);
    mySize = size;
}

