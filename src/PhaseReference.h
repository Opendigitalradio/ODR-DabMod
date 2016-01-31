/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#ifndef PHASE_REFERENCE_H
#define PHASE_REFERENCE_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModCodec.h"

#include <sys/types.h>
#include <complex>
#include <vector>


class PhaseReference : public ModCodec
{
public:
    PhaseReference(unsigned int dabmode);
    virtual ~PhaseReference();
    PhaseReference(const PhaseReference&);
    PhaseReference& operator=(const PhaseReference&);


    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "PhaseReference"; }

protected:
    unsigned int d_dabmode;
    size_t d_carriers;
    size_t d_num;
    const static unsigned char d_h[4][32];
    std::vector<std::complex<float> > d_dataIn;

    void fillData();
};

#endif // PHASE_REFERENCE_H

