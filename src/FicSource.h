/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "PuncturingRule.h"
#include "Eti.h"
#include "ModPlugin.h"
#include <vector>
#include <sys/types.h>

class FicSource : public ModInput
{
public:
    FicSource(eti_FC &fc);

    size_t getFramesize();
    const std::vector<PuncturingRule>& get_rules();

    void loadFicData(const Buffer& fic);
    int process(Buffer* outputData);
    const char* name() { return "FicSource"; }

private:
    size_t d_framesize;
    Buffer d_buffer;
    std::vector<PuncturingRule> d_puncturing_rules;
};

