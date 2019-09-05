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
#include "EtiReader.h"
#include "TimestampDecoder.h"

#include <string>
#include <cstdio>
#include <cstdint>
#include <memory>

class OutputFile : public ModOutput, public ModMetadata
{
public:
    OutputFile(const std::string& filename, bool show_metadata);

    virtual int process(Buffer* dataIn) override;
    const char* name() override { return "OutputFile"; }

    virtual meta_vec_t process_metadata(
            const meta_vec_t& metadataIn) override;

protected:
    bool myShowMetadata = false;
    frame_timestamp myLastTimestamp;
    std::string myFilename;

    struct FILEDeleter{ void operator()(FILE* fd){ if (fd) fclose(fd); }};
    std::unique_ptr<FILE, FILEDeleter> myFile;
};

