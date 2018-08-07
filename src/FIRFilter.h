/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

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
#   include <config.h>
#endif


#include "RemoteControl.h"
#include "ModPlugin.h"
#include "PcDebug.h"

#include <sys/types.h>
#include <complex>
#include <thread>
#include <vector>
#include <time.h>
#include <cstdio>
#include <string>
#include <memory>

#define FIRFILTER_PIPELINE_DELAY 1

typedef std::complex<float> complexf;

class FIRFilter : public PipelinedModCodec, public RemoteControllable
{
public:
    FIRFilter(std::string& taps_file);
    FIRFilter(const FIRFilter& other) = delete;
    FIRFilter& operator=(const FIRFilter& other) = delete;
    virtual ~FIRFilter();

    const char* name() override { return "FIRFilter"; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter,
            const std::string& value) override;

    virtual const std::string get_parameter(
            const std::string& parameter) const override;

protected:
    virtual int internal_process(Buffer* const dataIn, Buffer* dataOut) override;
    void load_filter_taps(const std::string &tapsFile);

    std::string& m_taps_file;

    mutable std::mutex m_taps_mutex;
    std::vector<float> m_taps;
};

