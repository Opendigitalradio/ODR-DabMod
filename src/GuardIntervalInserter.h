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

#ifndef GUARD_INTERVAL_INSERTER_H
#define GUARD_INTERVAL_INSERTER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"


#include <sys/types.h>


class GuardIntervalInserter : public ModCodec
{
public:
    GuardIntervalInserter(size_t nbSymbols, size_t spacing, size_t nullSize, size_t symSize);
    virtual ~GuardIntervalInserter();
    GuardIntervalInserter(const GuardIntervalInserter&);
    GuardIntervalInserter& operator=(const GuardIntervalInserter&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "GuardIntervalInserter"; }

protected:
    size_t d_nbSymbols;
    size_t d_spacing;
    size_t d_nullSize;
    size_t d_symSize;
    bool myHasNull;
};


#endif // GUARD_INTERVAL_INSERTER_H
