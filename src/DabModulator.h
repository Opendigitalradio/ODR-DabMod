/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2019
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
#include "ConfigParser.h"
#include "EtiReader.h"
#include "Flowgraph.h"
#include "GainControl.h"
#include "OutputMemory.h"
#include "RemoteControl.h"
#include "Log.h"
#include "TII.h"


class DabModulator : public ModInput, public ModMetadata, public RemoteControllable
{
public:
    DabModulator(EtiSource& etiSource, mod_settings_t& settings);

    int process(Buffer* dataOut) override;
    const char* name() override { return "DabModulator"; }

    virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn) override;

    /* Required to get the timestamp */
    EtiSource* getEtiSource() { return &myEtiSource; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter,
            const std::string& value) override;

    virtual const std::string get_parameter(
            const std::string& parameter) const override;

protected:
    void setMode(unsigned mode);

    mod_settings_t& m_settings;

    EtiSource& myEtiSource;
    std::shared_ptr<Flowgraph> myFlowgraph;

    size_t myNbSymbols;
    size_t myNbCarriers;
    size_t mySpacing;
    size_t myNullSize;
    size_t mySymSize;
    size_t myFicSizeOut;

    std::shared_ptr<OutputMemory> myOutput;
};

