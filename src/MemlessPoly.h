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
#include "ThreadsafeQueue.h"

#include <sys/types.h>
#include <complex>
#include <thread>
#include <vector>
#include <time.h>
#include <cstdio>
#include <string>
#include <memory>

#define MEMLESSPOLY_PIPELINE_DELAY 1

typedef std::complex<float> complexf;

class MemlessPoly : public PipelinedModCodec, public RemoteControllable
{
public:
    MemlessPoly(const std::string& coefs_am_file, const std::string& coefs_pm_file, unsigned int num_threads);

    virtual const char* name() { return "MemlessPoly"; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter,
            const std::string& value);

    virtual const std::string get_parameter(
            const std::string& parameter) const;

private:
    int internal_process(Buffer* const dataIn, Buffer* dataOut);
    void load_coefficients_am(const std::string &coefFile_am);
    void load_coefficients_pm(const std::string &coefFile_pm);

    unsigned int m_num_threads;
    std::vector<float> m_coefs_am;
    std::string m_coefs_am_file;
    mutable std::mutex m_coefs_am_mutex;

    std::vector<float> m_coefs_pm;
    std::string m_coefs_pm_file;
    mutable std::mutex m_coefs_pm_mutex;
};

