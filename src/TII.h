/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2015
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

#ifndef TII_H
#define TII_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModCodec.h"

#include <sys/types.h>
#include <complex>
#include <vector>
#include <string>

class TII : public ModCodec
{
    public:
        TII(unsigned int dabmode, unsigned int comb, unsigned int pattern);
        virtual ~TII();

        int process(Buffer* const dataIn, Buffer* dataOut);
        const char* name() { return m_name.c_str(); };

    protected:
        unsigned int m_dabmode;
        unsigned int m_comb;
        unsigned int m_pattern;

        bool m_insert;

        size_t m_carriers;

        std::string m_name;
        std::vector<std::complex<float> > m_dataIn;

        void prepare_pattern(void);

        void enable_carrier(int k);

    private:
        TII(const TII&);
        TII& operator=(const TII&);
};

#endif // TII_H

