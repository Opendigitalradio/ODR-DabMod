/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
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
#include "output/SDRDevice.h"

enum class FFTEngine {
    FFTW, // floating point in software
    KISS, // fixed-point in software
    DEXTER // fixed-point in FPGA
};

struct mod_settings_t {
    std::string startupCheck;

    std::string outputName;
    bool useZeroMQOutput = false;
    std::string zmqOutputSocketType = "";
    bool useFileOutput = false;
    std::string fileOutputFormat = "complexf";
    bool fileOutputShowMetadata = false;
    bool useUHDOutput = false;
    bool useSoapyOutput = false;
    bool useDexterOutput = false;
    bool useLimeOutput = false;
    bool useBladeRFOutput = false;

    FFTEngine fftEngine = FFTEngine::FFTW;

    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 1;
    float digitalgain = 1.0f;
    float normalise = 1.0f;
    GainMode gainMode = GainMode::GAIN_VAR;
    float gainmodeVariance = 4.0f;

    // To handle the timestamp offset of the modulator
    double tist_offset_s = 0.0;

    bool loop = false;
    std::string inputName = "";
    std::string inputTransport = "file";
    float edi_max_delay_ms = 0.0f;

    tii_config_t tiiConfig;

    std::string filterTapsFilename = "";

    std::string polyCoefFilename = "";
    unsigned polyNumThreads = 0;

    // Settings for crest factor reduction
    bool enableCfr = false;
    float cfrClip = 1.0f;
    float cfrErrorClip = 1.0f;

    // Settings for the OFDM windowing
    size_t ofdmWindowOverlap = 0;

    Output::SDRDeviceConfig sdr_device_config;

    bool showProcessTime = true;
};

void parse_args(int argc, char **argv, mod_settings_t& mod_settings);

