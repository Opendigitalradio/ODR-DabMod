/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "Buffer.h"
#include "ThreadsafeQueue.h"
#include <cstddef>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

// All flowgraph elements derive from ModPlugin, or a variant of it.
// Some ModPlugins also support handling metadata.

struct frame_timestamp;
struct flowgraph_metadata {
    std::shared_ptr<struct frame_timestamp> ts;
};

using meta_vec_t = std::vector<flowgraph_metadata>;

/* ModPlugins that support metadata derive from ModMetadata */
class ModMetadata {
    public:
        // Receives metadata from all inputs, and process them, and output
        // a sequence of metadata.
        virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn) = 0;
};


/* Abstract base class for all flowgraph elements */
class ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut) = 0;
    virtual const char* name() = 0;
    virtual ~ModPlugin() = default;
};

/* Inputs are sources, the output buffers without reading any */
class ModInput : public ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut);
    virtual int process(Buffer* dataOut) = 0;
};

/* Codecs are 1-input 1-output flowgraph plugins */
class ModCodec : public ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut);
    virtual int process(Buffer* const dataIn, Buffer* dataOut) = 0;
};

/* Pipelined ModCodecs run their processing in a separate thread, and
 * have a one-call-to-process() latency. Because of this latency, they
 * must also handle the metadata
 */
class PipelinedModCodec : public ModCodec, public ModMetadata
{
public:
    virtual int process(Buffer* const dataIn, Buffer* dataOut) final;
    virtual const char* name() = 0;

    virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn) final;

protected:
    // Once the instance implementing PipelinedModCodec has been constructed,
    // it must call start_pipeline_thread()
    void start_pipeline_thread(void);
    // To avoid race conditions on teardown, plugins must call
    // stop_pipeline_thread in their destructor.
    void stop_pipeline_thread(void);

    // The real processing must be implemented in internal_process
    virtual int internal_process(Buffer* const dataIn, Buffer* dataOut) = 0;

private:
    bool m_ready_to_output_data = false;

    ThreadsafeQueue<Buffer> m_input_queue;
    ThreadsafeQueue<Buffer> m_output_queue;

    std::deque<meta_vec_t> m_metadata_fifo;

    std::atomic<bool> m_running = ATOMIC_VAR_INIT(false);
    std::thread m_thread;
    void process_thread(void);
};


/* Muxes are N-input 1-output flowgraph plugins */
class ModMux : public ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut);
    virtual int process(std::vector<Buffer*> dataIn, Buffer* dataOut) = 0;
};

/* Outputs do not create any output buffers */
class ModOutput : public ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut);
    virtual int process(Buffer* dataIn) = 0;
};

