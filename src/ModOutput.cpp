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

#include "ModOutput.h"

#include <assert.h>


#ifdef DEBUG
size_t ModOutput::myCount = 0;
#endif


ModOutput::ModOutput(ModFormat inputFormat, ModFormat outputFormat) :
    ModPlugin((inputFormat), (outputFormat))
{
#ifdef DEBUG
    myOutputFile = NULL;
#endif
}


ModOutput::~ModOutput()
{
#ifdef DEBUG
    if (myOutputFile != NULL) {
        fclose(myOutputFile);
    }
#endif
}


int ModOutput::process(std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    assert(dataIn.size() == 1);
    assert(dataOut.size() == 0);

#ifdef DEBUG
    int ret = process(dataIn[0], NULL);
    if (myOutputFile == NULL) {
        char filename[256];
        sprintf(filename, "output.out.%.2zu.%s", myCount, name());
        myOutputFile = fopen(filename, "w");
        ++myCount;
        assert(myOutputFile != NULL);
    }
    fprintf(stderr, "Writting %zu bytes @ %p from %s\n", dataIn[0]->getLength(), dataIn[0], name());
    fwrite(dataIn[0]->getData(), dataIn[0]->getLength(), 1, myOutputFile);
    return ret;
#else
    return process(dataIn[0], NULL);
#endif
}
