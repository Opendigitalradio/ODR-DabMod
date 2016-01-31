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

#ifndef CIC_EQUALIZER_H
#define CIC_EQUALIZER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModCodec.h"

#include <vector>
#include <sys/types.h>
#include <complex>
#ifdef __SSE__
#   include <xmmintrin.h>
#endif


typedef std::complex<float> complexf;

class CicEqualizer : public ModCodec
{
public:
    CicEqualizer(size_t nbCarriers, size_t spacing, int R);
    virtual ~CicEqualizer();
    CicEqualizer(const CicEqualizer&);
    CicEqualizer& operator=(const CicEqualizer&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "CicEqualizer"; }

protected:
    size_t myNbCarriers;
    size_t mySpacing;
    std::vector<float> myFilter;
};


#endif //CIC_EQUALIZER_H

