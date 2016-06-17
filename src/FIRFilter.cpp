/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Written by
   2012, Matthias P. Braendli, matthias.braendli@mpb.li

   This block implements a FIR filter. The real filter taps are given
   as floats, and the block can take advantage of SSE.
   For better performance, filtering is done in another thread, leading
   to a pipeline delay of two calls to FIRFilter::process
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

#include "FIRFilter.h"
#include "PcDebug.h"
#include "Utils.h"

#include <stdio.h>
#include <stdexcept>

#include <iostream>
#include <fstream>
#include <memory>

#ifdef __AVX__
#   include <immintrin.h>
#else
#    ifdef __SSE__
#        include <xmmintrin.h>
#    endif
#endif

using namespace std;

#include <sys/time.h>

void FIRFilterWorker::process(struct FIRFilterWorkerData *fwd)
{
    size_t i;
    struct timespec time_start;
    struct timespec time_end;

    set_realtime_prio(1);
    set_thread_name("firfilter");

    // This thread creates the dataOut buffer, and deletes
    // the incoming buffer

    while(running) {
        std::shared_ptr<Buffer> dataIn;
        fwd->input_queue.wait_and_pop(dataIn);

        std::shared_ptr<Buffer> dataOut = make_shared<Buffer>();
        dataOut->setLength(dataIn->getLength());

        PDEBUG("FIRFilterWorker: dataIn->getLength() %zu\n", dataIn->getLength());

#if __AVX__
#define _mm256_load1_ps(x) _mm256_set_ps(x, x, x, x, x, x, x, x)
#warning FIRFilter uses experimental AVX code

        // The AVX accelerated version cannot work on the complex values,
        // it is necessary to do the convolution on the real and imaginary
        // parts separately. Thankfully, the taps are real, simplifying the
        // procedure.
        //
        // The AVX version is not enabled by default, because the performance
        // on my test machine (sandy bridge i7) is slightly worse with AVX than
        // with SSE. TODO: Try with Ivy Bridge or newer.
        //
        // Interesting links:
        // http://software.intel.com/en-us/forums/topic/283753

        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        if ((uintptr_t)(&out[0]) % 32 != 0) {
            fprintf(stderr, "FIRFilterWorker: out not aligned %p ", out);
            throw std::runtime_error("FIRFilterWorker: out not aligned");
        }

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_start);

        __m256 AVXout;
        __m256 AVXtaps;
        __m256 AVXin;
        {
            boost::mutex::scoped_lock lock(fwd->taps_mutex);

            for (i = 0; i < sizeIn - 2*fwd->taps.size(); i += 8) {
                AVXout = _mm256_setr_ps(0,0,0,0,0,0,0,0);

                for (size_t j = 0; j < fwd->taps.size; j++) {
                    if ((uintptr_t)(&in[i+2*j]) % 32 == 0) {
                        AVXin = _mm256_load_ps(&in[i+2*j]); //faster when aligned
                    }
                    else {
                        AVXin = _mm256_loadu_ps(&in[i+2*j]);
                    }

                    AVXtaps = _mm256_load1_ps(fwd->taps[j]);

                    AVXout = _mm256_add_ps(AVXout, _mm256_mul_ps(AVXin, AVXtaps));
                }
                _mm256_store_ps(&out[i], AVXout);
            }

            for (; i < sizeIn; i++) {
                out[i] = 0.0;
                for (int j = 0; i+2*j < sizeIn; j++) {
                    out[i] += in[i+2*j] * fwd->taps[j];
                }
            }
        }
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_end);

#elif __SSE__
        // The SSE accelerated version cannot work on the complex values,
        // it is necessary to do the convolution on the real and imaginary
        // parts separately. Thankfully, the taps are real, simplifying the
        // procedure.

        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        if ((uintptr_t)(&out[0]) % 16 != 0) {
            fprintf(stderr, "FIRFilterWorker: out not aligned %p ", out);
            throw std::runtime_error("FIRFilterWorker: out not aligned");
        }

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_start);

        __m128 SSEout;
        __m128 SSEtaps;
        __m128 SSEin;
        {
            boost::mutex::scoped_lock lock(fwd->taps_mutex);

            for (i = 0; i < sizeIn - 2*fwd->taps.size(); i += 4) {
                SSEout = _mm_setr_ps(0,0,0,0);

                for (size_t j = 0; j < fwd->taps.size(); j++) {
                    if ((uintptr_t)(&in[i+2*j]) % 16 == 0) {
                        SSEin = _mm_load_ps(&in[i+2*j]); //faster when aligned
                    }
                    else {
                        SSEin = _mm_loadu_ps(&in[i+2*j]);
                    }

                    SSEtaps = _mm_load1_ps(&fwd->taps[j]);

                    SSEout = _mm_add_ps(SSEout, _mm_mul_ps(SSEin, SSEtaps));
                }
                _mm_store_ps(&out[i], SSEout);
            }

            for (; i < sizeIn; i++) {
                out[i] = 0.0;
                for (int j = 0; i+2*j < sizeIn; j++) {
                    out[i] += in[i+2*j] * fwd->taps[j];
                }
            }
        }
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_end);

#else
        // No SSE ? Loop unrolling should make this faster. As for the SSE,
        // the real and imaginary parts are calculated separately.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_start);

        {
            boost::mutex::scoped_lock lock(fwd->taps_mutex);
            // Convolve by aligning both frame and taps at zero.
            for (i = 0; i < sizeIn - 2*fwd->taps.size(); i += 4) {
                out[i]    = 0.0;
                out[i+1]  = 0.0;
                out[i+2]  = 0.0;
                out[i+3]  = 0.0;

                for (size_t j = 0; j < fwd->taps.size(); j++) {
                    out[i]   += in[i   + 2*j] * fwd->taps[j];
                    out[i+1] += in[i+1 + 2*j] * fwd->taps[j];
                    out[i+2] += in[i+2 + 2*j] * fwd->taps[j];
                    out[i+3] += in[i+3 + 2*j] * fwd->taps[j];
                }
            }

            // At the end of the frame, we cut the convolution off.
            // The beginning of the next frame starts with a NULL symbol
            // anyway.
            for (; i < sizeIn; i++) {
                out[i] = 0.0;
                for (int j = 0; i+2*j < sizeIn; j++) {
                    out[i] += in[i+2*j] * fwd->taps[j];
                }
            }
        }

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_end);


#endif

        // The following implementations are for debugging only.
#if 0
        // Same thing as above, without loop unrolling. For debugging.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        for (i = 0; i < sizeIn - 2*fwd->taps.size(); i += 1) {
            out[i]  = 0.0;

            for (size_t j = 0; j < fwd->taps.size(); j++) {
                out[i]  += in[i+2*j] * fwd->taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; i+2*j < sizeIn; j++) {
                out[i] += in[i+2*j] * fwd->taps[j];
            }
        }

#elif 0
        // An unrolled loop, but this time, the input data is cast to complex float.
        // Makes indices more natural. For debugging.
        const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
        complexf* out      = reinterpret_cast<complexf*>(dataOut->getData());
        size_t sizeIn      = dataIn->getLength() / sizeof(complexf);

        for (i = 0; i < sizeIn - fwd->taps.size(); i += 4) {
            out[i]   = 0.0;
            out[i+1] = 0.0;
            out[i+2] = 0.0;
            out[i+3] = 0.0;

            for (size_t j = 0; j < fwd->taps.size(); j++) {
                out[i]   += in[i+j  ] * fwd->taps[j];
                out[i+1] += in[i+1+j] * fwd->taps[j];
                out[i+2] += in[i+2+j] * fwd->taps[j];
                out[i+3] += in[i+3+j] * fwd->taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; j+i < sizeIn; j++) {
                out[i] += in[i+j] * fwd->taps[j];
            }
        }

#elif 0
        // Simple implementation. Slow. For debugging.
        const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
        complexf* out      = reinterpret_cast<complexf*>(dataOut->getData());
        size_t sizeIn      = dataIn->getLength() / sizeof(complexf);

        for (i = 0; i < sizeIn - fwd->taps.size(); i += 1) {
            out[i]   = 0.0;

            for (size_t j = 0; j < fwd->taps.size(); j++) {
                out[i]  += in[i+j  ] * fwd->taps[j];
            }
        }

        for (; i < sizeIn; i++) {
            out[i] = 0.0;
            for (int j = 0; j+i < sizeIn; j++) {
                out[i] += in[i+j] * fwd->taps[j];
            }
        }
#endif

        calculationTime += (time_end.tv_sec - time_start.tv_sec) * 1000000000L +
            time_end.tv_nsec - time_start.tv_nsec;
        fwd->output_queue.push(dataOut);
    }
}


FIRFilter::FIRFilter(std::string& taps_file) :
    ModCodec(ModFormat(sizeof(complexf)), ModFormat(sizeof(complexf))),
    RemoteControllable("firfilter"),
    myTapsFile(taps_file)
{
    PDEBUG("FIRFilter::FIRFilter(%s) @ %p\n",
            taps_file.c_str(), this);

    RC_ADD_PARAMETER(ntaps, "(Read-only) number of filter taps.");
    RC_ADD_PARAMETER(tapsfile, "Filename containing filter taps. When written to, the new file gets automatically loaded.");

    number_of_runs = 0;

    load_filter_taps(myTapsFile);

#if __AVX__
    fprintf(stderr, "FIRFilter: WARNING: using experimental AVX code !\n");
#endif

    PDEBUG("FIRFilter: Starting worker\n" );
    worker.start(&firwd);
}

void FIRFilter::load_filter_taps(std::string tapsFile)
{
    std::ifstream taps_fstream(tapsFile.c_str());
    if(!taps_fstream) { 
        fprintf(stderr, "FIRFilter: file %s could not be opened !\n", tapsFile.c_str());
        throw std::runtime_error("FIRFilter: Could not open file with taps! ");
    }
    int n_taps;
    taps_fstream >> n_taps;

    if (n_taps <= 0) {
        fprintf(stderr, "FIRFilter: warning: taps file has invalid format\n");
        throw std::runtime_error("FIRFilter: taps file has invalid format.");
    }

    if (n_taps > 100) {
        fprintf(stderr, "FIRFilter: warning: taps file has more than 100 taps\n");
    }

    fprintf(stderr, "FIRFilter: Reading %d taps...\n", n_taps);

    std::vector<float> filter_taps(n_taps);

    int n;
    for (n = 0; n < n_taps; n++) {
        taps_fstream >> filter_taps[n];
        PDEBUG("FIRFilter: tap: %f\n",  filter_taps[n] );
        if (taps_fstream.eof()) {
            fprintf(stderr, "FIRFilter: file %s should contains %d taps, but EOF reached "\
                    "after %d taps !\n", tapsFile.c_str(), n_taps, n);
            throw std::runtime_error("FIRFilter: filtertaps file invalid ! ");
        }
    }

    {
        boost::mutex::scoped_lock lock(firwd.taps_mutex);

        firwd.taps = filter_taps;
    }
}


FIRFilter::~FIRFilter()
{
    PDEBUG("FIRFilter::~FIRFilter() @ %p\n", this);

    worker.stop();
}


int FIRFilter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FIRFilter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    // This thread creates the dataIn buffer, and deletes
    // the outgoing buffer

    std::shared_ptr<Buffer> inbuffer =
        make_shared<Buffer>(dataIn->getLength(), dataIn->getData());

    firwd.input_queue.push(inbuffer);

    if (number_of_runs > 2) {
        std::shared_ptr<Buffer> outbuffer;
        firwd.output_queue.wait_and_pop(outbuffer);

        dataOut->setData(outbuffer->getData(), outbuffer->getLength());
    }
    else {
        dataOut->setLength(dataIn->getLength());
        memset(dataOut->getData(), 0, dataOut->getLength());
        number_of_runs++;
    }

    return dataOut->getLength();

}

void FIRFilter::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "ntaps") {
        throw ParameterError("Parameter 'ntaps' is read-only");
    }
    else if (parameter == "tapsfile") {
        try {
            load_filter_taps(value);
            myTapsFile = value;
        }
        catch (std::runtime_error &e) {
            throw ParameterError(e.what());
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string FIRFilter::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "ntaps") {
        ss << firwd.taps.size();
    }
    else if (parameter == "tapsfile") {
        ss << myTapsFile;
    }
    else {
        ss << "Parameter '" << parameter << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

