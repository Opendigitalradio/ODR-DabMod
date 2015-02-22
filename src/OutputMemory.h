/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
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

#ifndef OUTPUT_MEMORY_H
#define OUTPUT_MEMORY_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

// This enables a rudimentary histogram functionality
// It gets printed when the OutputMemory gets destroyed
#define OUTPUT_MEM_HISTOGRAM 0

#if OUTPUT_MEM_HISTOGRAM
// The samples can go up to 100000 in value, make
// sure that HIST_BINS * HIST_BIN_SIZE is large
// enough !
#  define HIST_BINS 10
#  define HIST_BIN_SIZE 10000
#endif


#include "ModOutput.h"


class OutputMemory : public ModOutput
{
public:
    OutputMemory(Buffer* dataOut);
    virtual ~OutputMemory();
    virtual int process(Buffer* dataIn, Buffer* dataOut);
    const char* name() { return "OutputMemory"; }

    void setOutput(Buffer* dataOut);

protected:
    Buffer* myDataOut;

#if OUTPUT_MEM_HISTOGRAM
    // keep track of max value
    float    myMax;

    long int myHistogram[HIST_BINS];
#endif
};

#endif // OUTPUT_MEMORY_H

