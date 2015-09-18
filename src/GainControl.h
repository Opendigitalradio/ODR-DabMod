/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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

#ifndef GAIN_CONTROL_H
#define GAIN_CONTROL_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModCodec.h"
#include "RemoteControl.h"

#include <sys/types.h>
#include <complex>
#include <string>
#ifdef __SSE__
#   include <xmmintrin.h>
#endif


typedef std::complex<float> complexf;

enum class GainMode { GAIN_FIX = 0, GAIN_MAX = 1, GAIN_VAR = 2 };

class GainControl : public ModCodec, public RemoteControllable
{
    public:
        GainControl(size_t framesize,
                    GainMode mode,
                    float& digGain,
                    float normalise);

        virtual ~GainControl();
        GainControl(const GainControl&);
        GainControl& operator=(const GainControl&);

        int process(Buffer* const dataIn, Buffer* dataOut);
        const char* name() { return "GainControl"; }

        /* Functions for the remote control */
        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

    protected:
        size_t m_frameSize;
        float& m_digGain;
        float  m_normalise;
#ifdef __SSE__
        __m128 (*computeGain)(const __m128* in, size_t sizeIn);
        __m128 static computeGainFix(const __m128* in, size_t sizeIn);
        __m128 static computeGainMax(const __m128* in, size_t sizeIn);
        __m128 static computeGainVar(const __m128* in, size_t sizeIn);
#else
        float (*computeGain)(const complexf* in, size_t sizeIn);
        float static computeGainFix(const complexf* in, size_t sizeIn);
        float static computeGainMax(const complexf* in, size_t sizeIn);
        float static computeGainVar(const complexf* in, size_t sizeIn);
#endif
};

#endif // GAIN_CONTROL_H

