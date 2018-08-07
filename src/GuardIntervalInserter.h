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
#   include <config.h>
#endif

#include "ModPlugin.h"
#include "RemoteControl.h"
#include <stdint.h>
#include <vector>

/* The GuardIntervalInserter prepends the cyclic prefix to all
 * symbols in the transmission frame.
 *
 * If windowOverlap is non-zero, it will also add a cyclic suffix of
 * that length, enlarge the cyclic prefix too, and make symbols
 * overlap using a raised cosine window.
 * */
class GuardIntervalInserter : public ModCodec, public RemoteControllable
{
    public:
        GuardIntervalInserter(
                size_t nbSymbols,
                size_t spacing,
                size_t nullSize,
                size_t symSize,
                size_t& windowOverlap);

        int process(Buffer* const dataIn, Buffer* dataOut);
        const char* name() { return "GuardIntervalInserter"; }

        /******* REMOTE CONTROL ********/
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        virtual const std::string get_parameter(
                const std::string& parameter) const;

    protected:
        void update_window(size_t new_window_overlap);

        size_t d_nbSymbols;
        size_t d_spacing;
        size_t d_nullSize;
        size_t d_symSize;

        mutable std::mutex d_windowMutex;
        size_t& d_windowOverlap;
        std::vector<float> d_window;
};

