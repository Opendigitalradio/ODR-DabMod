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

#include <string>
#include "GainControl.h"
#include "TII.h"
#if defined(HAVE_OUTPUT_UHD)
#   include "OutputUHD.h"
#endif
#if defined(HAVE_SOAPYSDR)
#   include "OutputSoapy.h"
#endif

#define ZMQ_INPUT_MAX_FRAME_QUEUE 500

struct mod_settings_t {
    std::string outputName;
    int useZeroMQOutput = 0;
    std::string zmqOutputSocketType = "";
    int useFileOutput = 0;
    std::string fileOutputFormat = "complexf";
    int useUHDOutput = 0;
    int useSoapyOutput = 0;

    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 0;
    float digitalgain = 1.0f;
    float normalise = 1.0f;
    GainMode gainMode = GainMode::GAIN_VAR;

    // To handle the timestamp offset of the modulator
    unsigned tist_delay_stages = 0;
    double tist_offset_s = 0.0;

    bool loop = false;
    std::string inputName = "";
    std::string inputTransport = "file";
    unsigned inputMaxFramesQueued = ZMQ_INPUT_MAX_FRAME_QUEUE;
    float edi_max_delay_ms = 0.0f;

    tii_config_t tiiConfig;

    std::string filterTapsFilename = "";


#if defined(HAVE_OUTPUT_UHD)
    OutputUHDConfig outputuhd_conf;
#endif

#if defined(HAVE_SOAPYSDR)
    OutputSoapyConfig outputsoapy_conf;
#endif

};

void parse_args(int argc, char **argv, mod_settings_t& mod_settings);

