/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

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

#include "ModPlugin.h"
#include "PcDebug.h"
#include "Utils.h"
#include <stdexcept>
#include <string>
#include <cstring>

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

void PipelinedModCodec::stop_pipeline_thread()
{
    m_input_queue.push({});
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void PipelinedModCodec::start_pipeline_thread()
{
    m_running = true;
    m_thread = std::thread(&PipelinedModCodec::process_thread, this);
}

int PipelinedModCodec::process(Buffer* dataIn, Buffer* dataOut)
{
    if (!m_running) {
        return 0;
    }

    Buffer inbuffer;
    std::swap(inbuffer, *dataIn);
    m_input_queue.push(std::move(inbuffer));

    if (m_ready_to_output_data) {
        Buffer outbuffer;
        m_output_queue.wait_and_pop(outbuffer);
        std::swap(outbuffer, *dataOut);
    }
    else {
        dataOut->setLength(dataIn->getLength());
        if (dataOut->getLength() > 0) {
            memset(dataOut->getData(), 0, dataOut->getLength());
        }
        m_ready_to_output_data = true;
    }

    return dataOut->getLength();

}

meta_vec_t PipelinedModCodec::process_metadata(const meta_vec_t& metadataIn)
{
    m_metadata_fifo.push_back(metadataIn);
    if (m_metadata_fifo.size() == 2) {
        auto r = std::move(m_metadata_fifo.front());
        m_metadata_fifo.pop_front();
        return r;
    }
    else {
        return {};
    }
}

void PipelinedModCodec::process_thread()
{
    set_thread_name(name());
    set_realtime_prio(1);

    while (m_running) {
        Buffer dataIn;
        m_input_queue.wait_and_pop(dataIn);

        if (dataIn.getLength() == 0) {
            break;
        }

        Buffer dataOut;
        dataOut.setLength(dataIn.getLength());

        if (internal_process(&dataIn, &dataOut) == 0) {
            m_running = false;
        }

        m_output_queue.push(std::move(dataOut));
    }

    m_running = false;
}
