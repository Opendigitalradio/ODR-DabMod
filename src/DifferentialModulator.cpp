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

#include "DifferentialModulator.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <complex>
#include <string.h>

typedef std::complex<float> complexf;


DifferentialModulator::DifferentialModulator(size_t carriers) :
    ModMux(ModFormat(carriers * sizeof(complexf)), ModFormat(carriers * sizeof(complexf))),
    d_carriers(carriers)
{
    PDEBUG("DifferentialModulator::DifferentialModulator(%zu)\n", carriers);

}


DifferentialModulator::~DifferentialModulator()
{
    PDEBUG("DifferentialModulator::~DifferentialModulator()\n");

}


// dataIn[0] -> phase reference
// dataIn[1] -> data symbols
int DifferentialModulator::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
#ifdef DEBUG
    fprintf(stderr, "DifferentialModulator::process (dataIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %p", dataIn[i]);
    }
    fprintf(stderr, ", sizeIn: ");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %zu", dataIn[i]->getLength());
    }
    fprintf(stderr, ", dataOut: %p, sizeOut: %zu)\n", dataOut, dataOut->getLength());
#endif

    if (dataIn.size() != 2) {
        throw std::runtime_error(
                "DifferentialModulator::process nb of input streams not 2!");
    }

    size_t phaseSize = dataIn[0]->getLength() / sizeof(complexf);
    size_t dataSize = dataIn[1]->getLength() / sizeof(complexf);
    dataOut->setLength((phaseSize + dataSize) * sizeof(complexf));

    const complexf* phase = reinterpret_cast<const complexf*>(dataIn[0]->getData());
    const complexf* in = reinterpret_cast<const complexf*>(dataIn[1]->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());

    if (phaseSize != d_carriers) {
        throw std::runtime_error(
                "DifferentialModulator::process input phase size not valid!");
    }
    if (dataSize % d_carriers != 0) {
        throw std::runtime_error(
                "DifferentialModulator::process input data size not valid!");
    }

    memcpy(dataOut->getData(), phase, phaseSize * sizeof(complexf));
    for (size_t i = 0; i < dataSize; i += d_carriers) {
        for (size_t j = 0; j < d_carriers; j += 4) {
            out[d_carriers + j] = out[j] * in[j];
            out[d_carriers + j + 1] = out[j + 1] * in[j + 1];
            out[d_carriers + j + 2] = out[j + 2] * in[j + 2];
            out[d_carriers + j + 3] = out[j + 3] * in[j + 3];
        }
        in += d_carriers;
        out += d_carriers;
    }

    return dataOut->getLength();
}
