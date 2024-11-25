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

#include <cstdio>
#include <stdexcept>
#include <cstring>

DifferentialModulator::DifferentialModulator(size_t carriers, bool fixedPoint) :
    ModMux(),
    m_carriers(carriers),
    m_fixedPoint(fixedPoint)
{
    PDEBUG("DifferentialModulator::DifferentialModulator(%zu)\n", carriers);

}


DifferentialModulator::~DifferentialModulator()
{
    PDEBUG("DifferentialModulator::~DifferentialModulator()\n");
}


template<typename T>
void do_process(size_t carriers, const std::vector<Buffer*>& dataIn, Buffer* dataOut)
{
    size_t phaseSize = dataIn[0]->getLength() / sizeof(T);
    size_t dataSize = dataIn[1]->getLength() / sizeof(T);
    dataOut->setLength((phaseSize + dataSize) * sizeof(T));

    const T* phase = reinterpret_cast<const T*>(dataIn[0]->getData());
    const T* in = reinterpret_cast<const T*>(dataIn[1]->getData());
    T* out = reinterpret_cast<T*>(dataOut->getData());

    if (phaseSize != carriers) {
        throw std::runtime_error(
                "DifferentialModulator::process input phase size not valid!");
    }
    if (dataSize % carriers != 0) {
        throw std::runtime_error(
                "DifferentialModulator::process input data size not valid!");
    }

    memcpy(dataOut->getData(), phase, phaseSize * sizeof(T));
    for (size_t i = 0; i < dataSize; i += carriers) {
        for (size_t j = 0; j < carriers; j += 4) {
            out[carriers + j] = out[j] * in[j];
            out[carriers + j + 1] = out[j + 1] * in[j + 1];
            out[carriers + j + 2] = out[j + 2] * in[j + 2];
            out[carriers + j + 3] = out[j + 3] * in[j + 3];
        }
        in += carriers;
        out += carriers;
    }
}

// dataIn[0] -> phase reference
// dataIn[1] -> data symbols
int DifferentialModulator::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
#ifdef TRACE
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

    if (m_fixedPoint) {
        do_process<complexfix>(m_carriers, dataIn, dataOut);
    }
    else {
        do_process<complexf>(m_carriers, dataIn, dataOut);
    }

    return dataOut->getLength();
}
