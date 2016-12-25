/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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

#include "ModPlugin.h"
#include "PcDebug.h"
#include <stdexcept>
#include <string>

#define MODASSERT(cond) \
    if (not (cond)) { \
        throw std::runtime_error("Assertion failure: " #cond " for " + \
                std::string(name())); \
    }

int ModInput::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.empty());
    MODASSERT(dataOut.size() == 1);
    return process(dataOut[0]);
}

int ModCodec::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.size() == 1);
    MODASSERT(dataOut.size() == 1);
    return process(dataIn[0], dataOut[0]);
}

int ModMux::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(not dataIn.empty());
    MODASSERT(dataOut.size() == 1);
    return process(dataIn, dataOut[0]);
}

int ModOutput::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.size() == 1);
    MODASSERT(dataOut.empty());
    return process(dataIn[0]);
}

