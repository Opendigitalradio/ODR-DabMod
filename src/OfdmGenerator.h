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

#ifndef OFDM_GENERATOR_H
#define OFDM_GENERATOR_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "porting.h"
#include "ModCodec.h"
#include "kiss_fftsimd.h"


#include <kiss_fft.h>
#include <sys/types.h>


class OfdmGenerator : public ModCodec
{
public:
    OfdmGenerator(size_t nbSymbols, size_t nbCarriers, size_t spacing, bool inverse = true);
    virtual ~OfdmGenerator();
    OfdmGenerator(const OfdmGenerator&);
    OfdmGenerator& operator=(const OfdmGenerator&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "OfdmGenerator"; }

protected:
    FFT_PLAN myFftPlan;
    FFT_TYPE *myFftBuffer;
    size_t myNbSymbols;
    size_t myNbCarriers;
    size_t mySpacing;
    unsigned myPosSrc;
    unsigned myPosDst;
    unsigned myPosSize;
    unsigned myNegSrc;
    unsigned myNegDst;
    unsigned myNegSize;
    unsigned myZeroDst;
    unsigned myZeroSize;
};


#endif // OFDM_GENERATOR_H
