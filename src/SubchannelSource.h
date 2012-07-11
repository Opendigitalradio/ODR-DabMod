/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SUBCHANNEL_SOURCE_H
#define SUBCHANNEL_SOURCE_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "PuncturingRule.h"
#include "Eti.h"
#include "ModInput.h"

#include <vector>


class SubchannelSource : public ModInput
{
protected:
    size_t d_start_address;
    size_t d_framesize;
    size_t d_protection;
    Buffer d_buffer;
    std::vector<PuncturingRule*> d_puncturing_rules;
    
public:
    SubchannelSource(eti_STC &stc);
    SubchannelSource(const SubchannelSource&);
    SubchannelSource& operator=(const SubchannelSource&);
    virtual ~SubchannelSource();

    size_t startAddress();
    size_t framesize();
    size_t framesizeCu();
    size_t bitrate();
    size_t protection();
    size_t protectionForm();
    size_t protectionLevel();
    size_t protectionOption();
    const std::vector<PuncturingRule*>& get_rules();
    
    int process(Buffer* inputData, Buffer* outputData);
    const char* name() { return "SubchannelSource"; }
    int read(Buffer* outputData);
};


#endif // SUBCHANNEL_SOURCE_H
