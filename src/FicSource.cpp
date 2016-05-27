/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#include "FicSource.h"
#include "PcDebug.h"

#include <stdexcept>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>


const std::vector<PuncturingRule*>& FicSource::get_rules()
{
    return d_puncturing_rules;
}


FicSource::FicSource(eti_FC &fc) :
    ModInput(ModFormat(0), ModFormat(0))
{
//    PDEBUG("FicSource::FicSource(...)\n");
//    PDEBUG("  Start address: %i\n", d_start_address);
//    PDEBUG("  Framesize: %i\n", d_framesize);
//    PDEBUG("  Protection: %i\n", d_protection);

    if (fc.FICF == 0) {
        d_framesize = 0;
        d_buffer.setLength(0);
        return;
    }
    
    if (fc.MID == 3) {
        d_framesize = 32 * 4;
        d_puncturing_rules.push_back(new PuncturingRule(29 * 16, 0xeeeeeeee));
        d_puncturing_rules.push_back(new PuncturingRule(3 * 16, 0xeeeeeeec));
    } else {
        d_framesize = 24 * 4;
        d_puncturing_rules.push_back(new PuncturingRule(21 * 16, 0xeeeeeeee));
        d_puncturing_rules.push_back(new PuncturingRule(3 * 16, 0xeeeeeeec));
    }
    d_buffer.setLength(d_framesize);

    myOutputFormat.size(d_framesize);
}


FicSource::~FicSource()
{
    PDEBUG("FicSource::~FicSource()\n");
    for (size_t i = 0; i < d_puncturing_rules.size(); ++i) {
//        PDEBUG(" Deleting rules @ %p\n", d_puncturing_rules[i]);
        delete d_puncturing_rules[i];
    }
}


size_t FicSource::getFramesize()
{
    return d_framesize;
}


int FicSource::process(Buffer* inputData, Buffer* outputData)
{
    PDEBUG("FicSource::process"
            "(inputData: %p, outputData: %p)\n",
            inputData, outputData);

    if ((inputData != NULL) && inputData->getLength()) {
        PDEBUG(" Input, storing data\n");
        if (inputData->getLength() != d_framesize) {
            PDEBUG("ERROR: FicSource::process.inputSize != d_framesize\n");
            exit(-1);
        }
        d_buffer = *inputData;
        return inputData->getLength();
    }
    PDEBUG(" Output, retriving data\n");
    
    return read(outputData);
}


int FicSource::read(Buffer* outputData)
{
    PDEBUG("FicSource::read(outputData: %p, outputSize: %zu)\n",
            outputData, outputData->getLength());

    if (d_buffer.getLength() != d_framesize) {
        PDEBUG("ERROR: FicSource::read.outputSize != d_framesize\n");
        exit(-1);
    }
    *outputData = d_buffer;
    
    return outputData->getLength();
}
