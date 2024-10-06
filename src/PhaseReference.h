/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModPlugin.h"

#include <vector>
#include <cstddef>

template <typename T>
struct PhaseRefGen {
    std::vector<T> dataIn;
    void fillData(unsigned int dabmode, size_t carriers);

    private:
    T convert(uint8_t data);
};


class PhaseReference : public ModInput
{
    public:
        PhaseReference(unsigned int dabmode, bool fixedPoint);

        int process(Buffer* dataOut) override;
        const char* name() override { return "PhaseReference"; }

    protected:
        unsigned int d_dabmode;
        bool d_fixedPoint;
        size_t d_carriers;

        PhaseRefGen<complexf> d_phaseRefCF32;
        PhaseRefGen<complexfix> d_phaseRefFixed;
};

