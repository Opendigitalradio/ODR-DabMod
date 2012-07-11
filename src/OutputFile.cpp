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

#include "OutputFile.h"
#include "PcDebug.h"

#include <assert.h>
#include <stdexcept>


OutputFile::OutputFile(char* filename) :
    ModOutput(ModFormat(1), ModFormat(0)),
    myFilename(filename)
{
    PDEBUG("OutputFile::OutputFile(filename: %s) @ %p\n",
            filename, this);

    myFile = fopen(filename, "w");
    if (myFile == NULL) {
        perror(filename);
        throw std::runtime_error(
                "OutputFile::OutputFile() unable to open file!");
    }
}


OutputFile::~OutputFile()
{
    PDEBUG("OutputFile::~OutputFile() @ %p\n", this);

    if (myFile != NULL) {
        fclose(myFile);
    }
}


int OutputFile::process(Buffer* dataIn, Buffer* dataOut)
{
    PDEBUG("OutputFile::process(%p, %p)\n", dataIn, dataOut);
    assert(dataIn != NULL);

    if (fwrite(dataIn->getData(), dataIn->getLength(), 1, myFile) == 0) {
        throw std::runtime_error(
                "OutputFile::process() unable to write to file!");
    }

    return dataIn->getLength();
}
