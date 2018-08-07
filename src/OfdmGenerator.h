/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

#include "ModPlugin.h"
#include "RemoteControl.h"
#include "PAPRStats.h"
#include "fftw3.h"
#include <cstddef>
#include <vector>
#include <complex>
#include <atomic>

typedef std::complex<float> complexf;

class OfdmGenerator : public ModCodec, public RemoteControllable
{
    public:
        OfdmGenerator(size_t nbSymbols,
                      size_t nbCarriers,
                      size_t spacing,
                      bool& enableCfr,
                      float& cfrClip,
                      float& cfrErrorClip,
                      bool inverse = true);
        virtual ~OfdmGenerator();
        OfdmGenerator(const OfdmGenerator&) = delete;
        OfdmGenerator& operator=(const OfdmGenerator&) = delete;

        int process(Buffer* const dataIn, Buffer* dataOut) override;
        const char* name() override { return "OfdmGenerator"; }

        /* Functions for the remote control */
        /* Base function to set parameters. */
        virtual void set_parameter(
                const std::string& parameter,
                const std::string& value) override;

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const override;

    protected:
        struct cfr_iter_stat_t {
            size_t clip_count = 0;
            size_t errclip_count = 0;
        };

        cfr_iter_stat_t cfr_one_iteration(
                complexf *symbol, const complexf *reference);

        fftwf_plan myFftPlan;
        fftwf_complex *myFftIn, *myFftOut;
        const size_t myNbSymbols;
        const size_t myNbCarriers;
        const size_t mySpacing;
        unsigned myPosSrc;
        unsigned myPosDst;
        unsigned myPosSize;
        unsigned myNegSrc;
        unsigned myNegDst;
        unsigned myNegSize;
        unsigned myZeroDst;
        unsigned myZeroSize;

        bool& myCfr; // Whether to enable crest factor reduction
        mutable std::mutex myCfrRcMutex;
        float& myCfrClip;
        float& myCfrErrorClip;
        fftwf_plan myCfrFft;
        fftwf_complex *myCfrPostClip;
        fftwf_complex *myCfrPostFft;

        // Statistics for CFR
        std::deque<double> myClipRatios;
        std::deque<double> myErrorClipRatios;

        // Measure PAPR before and after CFR
        PAPRStats myPaprBeforeCFR;
        PAPRStats myPaprAfterCFR;
        std::atomic<bool> myPaprClearRequest;

        size_t myMERCalcIndex = 0;
        std::deque<double> myMERs;
};


