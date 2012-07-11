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

#include "FIRFilter.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>

#include <iostream>
#include <fstream>

#ifdef __SSE__
#   include <xmmintrin.h>
#endif


#include <sys/time.h>

void FIRFilterWorker::process(struct FIRFilterWorkerData *fwd)
{
    size_t i;
    struct timespec time_start;
    struct timespec time_end;

    // This thread creates the dataOut buffer, and deletes
    // the incoming buffer

    while(running) {
        Buffer* dataIn;
        fwd->input_queue.wait_and_pop(dataIn);

        Buffer* dataOut;
        dataOut = new Buffer();
        dataOut->setLength(dataIn->getLength());

        PDEBUG("FIRFilterWorker: dataIn->getLength() %d\n", dataIn->getLength());

#if __SSE__
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
        for (i = 0; i < sizeIn - 2*fwd->n_taps; i += 4) {
            SSEout = _mm_setr_ps(0,0,0,0);

            for (int j = 0; j < fwd->n_taps; j++) {
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
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_end);

#else
        // No SSE ? Loop unrolling should make this faster. As for the SSE,
        // the real and imaginary parts are calculated separately.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_start);

        // Convolve by aligning both frame and taps at zero.
        for (i = 0; i < sizeIn - 2*fwd->n_taps; i += 4) {
            out[i]    = 0.0;
            out[i+1]  = 0.0;
            out[i+2]  = 0.0;
            out[i+3]  = 0.0;

            for (int j = 0; j < fwd->n_taps; j++) {
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

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_end);


#endif

        // The following implementations are for debugging only.
#if 0
        // Same thing as above, without loop unrolling. For debugging.
        const float* in = reinterpret_cast<const float*>(dataIn->getData());
        float* out      = reinterpret_cast<float*>(dataOut->getData());
        size_t sizeIn   = dataIn->getLength() / sizeof(float);

        for (i = 0; i < sizeIn - 2*fwd->n_taps; i += 1) {
            out[i]  = 0.0;

            for (int j = 0; j < fwd->n_taps; j++) {
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

        for (i = 0; i < sizeIn - fwd->n_taps; i += 4) {
            out[i]   = 0.0;
            out[i+1] = 0.0;
            out[i+2] = 0.0;
            out[i+3] = 0.0;

            for (int j = 0; j < fwd->n_taps; j++) {
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

        for (i = 0; i < sizeIn - fwd->n_taps; i += 1) {
            out[i]   = 0.0;

            for (int j = 0; j < fwd->n_taps; j++) {
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
        delete dataIn;
    }
}


FIRFilter::FIRFilter(char* taps_file) :
    ModCodec(ModFormat(sizeof(complexf)), ModFormat(sizeof(complexf)))
{
    PDEBUG("FIRFilter::FIRFilter(%s) @ %p\n",
            taps_file, this);

    number_of_runs = 0;

    std::ifstream taps_fstream(taps_file);
    if(!taps_fstream) { 
        fprintf(stderr, "FIRFilter: file %s could not be opened !\n", taps_file);
        throw std::runtime_error("FIRFilter: Could not open file with taps! ");
    }
    int n_taps;
    taps_fstream >> n_taps;

    my_Ntaps = n_taps;

    fprintf(stderr, "FIRFilter: Reading %d taps...\n", my_Ntaps);

    myFilter = new float[my_Ntaps];

    int n;
    for (n = 0; n < n_taps; n++) {
        taps_fstream >> myFilter[n];
        PDEBUG("FIRFilter: tap: %f\n",  myFilter[n] );
        if (taps_fstream.eof()) {
            fprintf(stderr, "FIRFilter: file %s should contains %d taps, but EOF reached "\
                    "after %d taps !\n", taps_file, n_taps, n);
            throw std::runtime_error("FIRFilter: filtertaps file invalid ! ");
        }
    }

    firwd.taps = myFilter;
    firwd.n_taps = my_Ntaps;

    PDEBUG("FIRFilter: Starting worker\n" );
    worker.start(&firwd);
}


FIRFilter::~FIRFilter()
{
    PDEBUG("FIRFilter::~FIRFilter() @ %p\n", this);

    worker.stop();

    if (myFilter != NULL) {
        delete[] myFilter;
    }
}


int FIRFilter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FIRFilter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    // This thread creates the dataIn buffer, and deletes
    // the outgoing buffer

    Buffer* inbuffer = new Buffer(dataIn->getLength(), dataIn->getData());

    firwd.input_queue.push(inbuffer);

    if (number_of_runs > 2) {
        Buffer* outbuffer;
        firwd.output_queue.wait_and_pop(outbuffer);

        dataOut->setData(outbuffer->getData(), outbuffer->getLength());

        delete outbuffer;
    }
    else {
        dataOut->setLength(dataIn->getLength());
        memset(dataOut->getData(), 0, dataOut->getLength());
        number_of_runs++;
    }

    return dataOut->getLength();

}
