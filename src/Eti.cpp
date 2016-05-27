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

#ifdef _WIN32
#   pragma warning ( disable : 4103 )
#   include "Eti.h"
#   pragma warning ( default : 4103 )
#else
#   include "Eti.h"
#endif


//definitions des structures des champs du ETI(NI, G703)


uint16_t eti_FC::getFrameLength()
{
    return (uint16_t)((FL_high << 8) | FL_low);
}


void eti_FC::setFrameLength(uint16_t length)
{
    FL_high = (length >> 8) & 0x07;
    FL_low = length & 0xff;
}


void eti_STC::setSTL(uint16_t length)
{
    STL_high = length >> 8;
    STL_low = length & 0xff;
}


uint16_t eti_STC::getSTL()
{
    return (uint16_t)((STL_high << 8) + STL_low);
}


void eti_STC::setStartAddress(uint16_t address)
{
    startAddress_high = address >> 8;
    startAddress_low = address & 0xff;
}


uint16_t eti_STC::getStartAddress()
{
    return (uint16_t)((startAddress_high << 8) + startAddress_low);
}
