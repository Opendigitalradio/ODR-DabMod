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

#include "ModInput.h"

#include <assert.h>


#ifdef DEBUG
size_t ModInput::myCount = 0;
#endif


ModInput::ModInput(ModFormat inputFormat, ModFormat outputFormat) :
    ModPlugin((inputFormat), (outputFormat))
{
#ifdef DEBUG
    myOutputFile = NULL;
#endif
}


ModInput::~ModInput()
{
#ifdef DEBUG
    if (myOutputFile != NULL) {
        fclose(myOutputFile);
    }
#endif
}


int ModInput::process(std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    assert(dataIn.size() == 0);
    assert(dataOut.size() == 1);

#ifdef DEBUG
    int ret = process(NULL, dataOut[0]);
    if (myOutputFile == NULL) {
        char filename[256];
        sprintf(filename, "output.in.%.2zu.%s", myCount, name());
        myOutputFile = fopen(filename, "w");
        ++myCount;
        assert(myOutputFile != NULL);
    }
    fprintf(stderr, "Writting %zu bytes @ %p from %s\n", dataOut[0]->getLength(), dataOut[0], name());
    fwrite(dataOut[0]->getData(), dataOut[0]->getLength(), 1, myOutputFile);
    return ret;
#else
    return process(NULL, dataOut[0]);
#endif
}
