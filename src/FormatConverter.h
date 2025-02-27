/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2022
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

    This flowgraph block converts complexf to signed integer.
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

#include "ModPlugin.h"
#include <atomic>
#include <string>

class FormatConverter : public ModCodec
{
    public:
        static size_t get_format_size(const std::string& format);

        // floating-point input allows output formats: s8, u8 and s16
        // complexfix_wide input allows output formats: s16
        // complexfix input is already in s16, and needs no converter
        FormatConverter(bool input_is_complexfix_wide, const std::string& format_out);
        virtual ~FormatConverter();

        int process(Buffer* const dataIn, Buffer* dataOut);
        const char* name();

        size_t get_num_clipped_samples() const;

    private:
        bool m_input_complexfix_wide;
        std::string m_format_out;

        std::atomic<size_t> m_num_clipped_samples = 0;
};


