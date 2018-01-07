/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "Buffer.h"
#include "ThreadsafeQueue.h"

#include <sys/types.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

class ModPlugin
{
public:
    virtual int process(
            std::vector<Buffer*> dataIn,
            std::vector<Buffer*> dataOut) = 0;
    virtual const char* name() = 0;
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

class PipelinedModCodec : public ModCodec
{
public:
    PipelinedModCodec();
    PipelinedModCodec(const PipelinedModCodec&) = delete;
    PipelinedModCodec& operator=(const PipelinedModCodec&) = delete;
    PipelinedModCodec(PipelinedModCodec&&) = delete;
    PipelinedModCodec& operator=(PipelinedModCodec&&) = delete;
    ~PipelinedModCodec();

    virtual int process(Buffer* const dataIn, Buffer* dataOut) final;
    virtual const char* name() = 0;

protected:
    // Once the instance implementing PipelinedModCodec has been constructed,
    // it must call start_pipeline_thread()
    void start_pipeline_thread(void);
    virtual int internal_process(Buffer* const dataIn, Buffer* dataOut) = 0;

private:
    size_t m_number_of_runs;

    ThreadsafeQueue<std::shared_ptr<Buffer> > m_input_queue;
    ThreadsafeQueue<std::shared_ptr<Buffer> > m_output_queue;

    std::atomic<bool> m_running;
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

struct frame_timestamp;
struct flowgraph_metadata {
    std::shared_ptr<struct frame_timestamp> ts;
};


using meta_vec_t = std::vector<flowgraph_metadata>;

/* Some ModPlugins also support metadata */
class ModMetadata {
    public:
        // Receives metadata from all inputs, and process them, and output
        // a sequence of metadata.
        virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn) = 0;
};

