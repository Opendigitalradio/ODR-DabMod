/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "ModPlugin.h"
#include "PcDebug.h"
#include "Utils.h"
#include <stdexcept>
#include <string>

#define MODASSERT(cond) \
    if (not (cond)) { \
        throw std::runtime_error("Assertion failure: " #cond " for " + \
                std::string(name())); \
    }

int ModInput::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.empty());
    MODASSERT(dataOut.size() == 1);
    return process(dataOut[0]);
}

int ModCodec::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.size() == 1);
    MODASSERT(dataOut.size() == 1);
    return process(dataIn[0], dataOut[0]);
}

int ModMux::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(not dataIn.empty());
    MODASSERT(dataOut.size() == 1);
    return process(dataIn, dataOut[0]);
}

int ModOutput::process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut)
{
    MODASSERT(dataIn.size() == 1);
    MODASSERT(dataOut.empty());
    return process(dataIn[0]);
}

PipelinedModCodec::PipelinedModCodec() :
    ModCodec(),
    m_input_queue(),
    m_output_queue(),
    m_number_of_runs(0),
    m_thread(&PipelinedModCodec::process_thread, this)
{
}

PipelinedModCodec::~PipelinedModCodec()
{
    m_input_queue.push({});
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

int PipelinedModCodec::process(Buffer* dataIn, Buffer* dataOut)
{
    if (!m_running) {
        return 0;
    }

    std::shared_ptr<Buffer> inbuffer =
        std::make_shared<Buffer>(dataIn->getLength(), dataIn->getData());

    m_input_queue.push(inbuffer);

    if (m_number_of_runs > 0) {
        std::shared_ptr<Buffer> outbuffer;
        m_output_queue.wait_and_pop(outbuffer);

        dataOut->setData(outbuffer->getData(), outbuffer->getLength());
    }
    else {
        dataOut->setLength(dataIn->getLength());
        memset(dataOut->getData(), 0, dataOut->getLength());
        m_number_of_runs++;
    }

    return dataOut->getLength();

}

void PipelinedModCodec::process_thread()
{
    set_thread_name(name());
    set_realtime_prio(1);

    while (m_running) {
        std::shared_ptr<Buffer> dataIn;
        m_input_queue.wait_and_pop(dataIn);

        if (!dataIn or dataIn->getLength() == 0) {
            break;
        }

        std::shared_ptr<Buffer> dataOut = std::make_shared<Buffer>();
        dataOut->setLength(dataIn->getLength());

        if (internal_process(dataIn.get(), dataOut.get()) == 0) {
            m_running = false;
        }

        m_output_queue.push(dataOut);
    }

    m_running = false;
}
