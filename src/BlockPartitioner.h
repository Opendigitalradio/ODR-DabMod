/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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
#include <vector>
#include <cstddef>

class BlockPartitioner : public ModMux, public ModMetadata
{
public:
    BlockPartitioner(unsigned mode);

    int process(std::vector<Buffer*> dataIn, Buffer* dataOut);
    const char* name() { return "BlockPartitioner"; }

    // The implementation assumes process_metadata is always called after process
    virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn);

protected:
    int d_mode;
    size_t d_ficSize;
    size_t d_cifCount;
    size_t d_cifNb = 0;
    const size_t d_cifSize = 864 * 8;
    size_t d_outputFramesize;
    size_t d_outputFramecount;

    meta_vec_t d_meta;
};

