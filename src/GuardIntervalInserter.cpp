/*
   Copyright (C) 2005, 2206, 2007, 2008, 2009, 2010, 2011 Her Majesty
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

#include "GuardIntervalInserter.h"
#include "PcDebug.h"
#include <string.h>
#include <stdexcept>
#include <complex>

typedef std::complex<float> complexf;

GuardIntervalInserter::GuardIntervalInserter(size_t nbSymbols,
        size_t spacing,
        size_t nullSize,
        size_t symSize) :
    ModCodec(),
    d_nbSymbols(nbSymbols),
    d_spacing(spacing),
    d_nullSize(nullSize),
    d_symSize(symSize)
{
    PDEBUG("GuardIntervalInserter::GuardIntervalInserter(%zu, %zu, %zu, %zu)"
           " @ %p\n", nbSymbols, spacing, nullSize, symSize, this);
}


int GuardIntervalInserter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("GuardIntervalInserter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength((d_nullSize + (d_nbSymbols * d_symSize))
            * sizeof(complexf));

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);

    if (sizeIn != (d_nbSymbols + (d_nullSize ? 1 : 0)) * d_spacing) {
        PDEBUG("Nb symbols: %zu\n", d_nbSymbols);
        PDEBUG("Spacing: %zu\n", d_spacing);
        PDEBUG("Null size: %zu\n", d_nullSize);
        PDEBUG("Sym size: %zu\n", d_symSize);
        PDEBUG("\n%zu != %zu\n", sizeIn, (d_nbSymbols + 1) * d_spacing);
        throw std::runtime_error(
                "GuardIntervalInserter::process input size not valid!");
    }

    // Handle Null symbol separately because it is longer
    if (d_nullSize) {
        // end - (nullSize - spacing) = 2 * spacing - nullSize
        memcpy(out, &in[2 * d_spacing - d_nullSize],
                (d_nullSize - d_spacing) * sizeof(complexf));
        memcpy(&out[d_nullSize - d_spacing], in, d_spacing * sizeof(complexf));
        in += d_spacing;
        out += d_nullSize;
    }

    // Data symbols
    for (size_t i = 0; i < d_nbSymbols; ++i) {
        // end - (symSize - spacing) = 2 * spacing - symSize
        memcpy(out, &in[2 * d_spacing - d_symSize],
                (d_symSize - d_spacing) * sizeof(complexf));
        memcpy(&out[d_symSize - d_spacing], in, d_spacing * sizeof(complexf));
        in += d_spacing;
        out += d_symSize;
    }

    return sizeIn;
}
