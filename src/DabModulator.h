/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2017
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
#   include "config.h"
#endif

#include <sys/types.h>
#include <string>
#include <memory>

#include "ModPlugin.h"
#include "EtiReader.h"
#include "Flowgraph.h"
#include "GainControl.h"
#include "OutputMemory.h"
#include "RemoteControl.h"
#include "Log.h"
#include "TII.h"


class DabModulator : public ModInput
{
public:
    DabModulator(
            EtiSource& etiSource,
            tii_config_t& tiiConfig,
            unsigned outputRate, unsigned clockRate,
            unsigned dabMode, GainMode gainMode,
            float& digGain, float normalise,
            float gainmodeVariance,
            const std::string& filterTapsFilename,
            const std::string& polyCoefFilename);
    DabModulator(const DabModulator& other) = delete;
    DabModulator& operator=(const DabModulator& other) = delete;
    virtual ~DabModulator();

    int process(Buffer* dataOut);
    const char* name() { return "DabModulator"; }

    /* Required to get the timestamp */
    EtiSource* getEtiSource() { return &myEtiSource; }

protected:
    void setMode(unsigned mode);

    unsigned myOutputRate;
    unsigned myClockRate;
    unsigned myDabMode;
    GainMode myGainMode;
    float& myDigGain;
    float myNormalise;
    float myGainmodeVariance;
    EtiSource& myEtiSource;
    Flowgraph* myFlowgraph;
    OutputMemory* myOutput;
    std::string myFilterTapsFilename;
    std::string myPolyCoefFilename;
    tii_config_t& myTiiConfig;

    size_t myNbSymbols;
    size_t myNbCarriers;
    size_t mySpacing;
    size_t myNullSize;
    size_t mySymSize;
    size_t myFicSizeOut;
    size_t myFicSizeIn;
};

