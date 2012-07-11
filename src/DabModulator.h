/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
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

#ifndef DAB_MODULATOR_H
#define DAB_MODULATOR_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <sys/types.h>

#include "ModCodec.h"
#include "EtiReader.h"
#include "Flowgraph.h"
#include "GainControl.h"
#include "OutputMemory.h"


class DabModulator : public ModCodec
{
public:
    DabModulator(unsigned outputRate = 2048000, unsigned clockRate = 0,
            unsigned dabMode = 0, GainMode gainMode = GAIN_VAR, float factor = 1.0);
    DabModulator(const DabModulator& copy);
    virtual ~DabModulator();

    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "DabModulator"; }

protected:
    void setMode(unsigned mode);

    unsigned myOutputRate;
    unsigned myClockRate;
    unsigned myDabMode;
    GainMode myGainMode;
    float myFactor;
    EtiReader myEtiReader;
    Flowgraph* myFlowgraph;
    OutputMemory* myOutput;

    size_t myNbSymbols;
    size_t myNbCarriers;
    size_t mySpacing;
    size_t myNullSize;
    size_t mySymSize;
    size_t myFicSizeOut;
    size_t myFicSizeIn;
};


#endif // DAB_MODULATOR_H
