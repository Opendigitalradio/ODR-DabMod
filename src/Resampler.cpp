/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
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

#include "Resampler.h"
#include "PcDebug.h"

#include <malloc.h>
#include <sys/types.h>
#include <string.h>
#include <stdexcept>
#include <assert.h>

#if USE_FFTW
#  define FFT_REAL(x) x[0]
#  define FFT_IMAG(x) x[1]
#endif

unsigned gcd(unsigned a, unsigned b)
{
    if (b == 0) {
        return a;
    }

    return gcd(b, a % b);
}


Resampler::Resampler(size_t inputRate, size_t outputRate, size_t resolution) :
    ModCodec(ModFormat(inputRate * sizeof(complexf)),
            ModFormat(outputRate * sizeof(complexf))),
    myFftPlan1(NULL),
    myFftPlan2(NULL),
    myFftIn(NULL),
    myFftOut(NULL),
    myBufferIn(NULL),
    myBufferOut(NULL),
    myFront(NULL),
    myBack(NULL),
    myWindow(NULL)
{
    PDEBUG("Resampler::Resampler(%zu, %zu) @ %p\n", inputRate, outputRate, this);

#if USE_FFTW
    fprintf(stderr, "This software uses the FFTW library.\n\n");
#else
    fprintf(stderr, "This software uses KISS FFT.\n\n");
    fprintf(stderr, "Copyright (c) 2003-2004 Mark Borgerding\n"
            "\n"
            "All rights reserved.\n"
            "\n"
            "Redistribution and use in source and binary forms, with or "
            "without modification, are permitted provided that the following "
            "conditions are met:\n"
            "\n"
            "    * Redistributions of source code must retain the above "
            "copyright notice, this list of conditions and the following "
            "disclaimer.\n"
            "    * Redistributions in binary form must reproduce the above "
            "copyright notice, this list of conditions and the following "
            "disclaimer in the documentation and/or other materials provided "
            "with the distribution.\n"
            "    * Neither the author nor the names of any contributors may be "
            "used to endorse or promote products derived from this software "
            "without specific prior written permission.\n"
            "\n"
            "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND "
            "CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, "
            "INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF "
            "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE "
            "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS "
            "BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, "
            "EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED "
            "TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, "
            "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON "
            "ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, "
            "OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY "
            "OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE "
            "POSSIBILITY OF SUCH DAMAGE.\n");
#endif

    size_t divisor = gcd(inputRate, outputRate);
    L = outputRate / divisor;
    M = inputRate / divisor;
    PDEBUG(" gcd: %zu, L: %zu, M: %zu\n", divisor, L, M);
    {
        unsigned factor = resolution * 2 / M;
        if (factor & 1) {
            ++factor;
        }
        myFftSizeIn = factor * M;
        myFftSizeOut = factor * L;
    }
    PDEBUG(" FFT size in: %zu, FFT size out: %zu\n", myFftSizeIn, myFftSizeOut);

    if (myFftSizeIn > myFftSizeOut) {
        myFactor = 1.0f / myFftSizeIn;
    } else {
        myFactor = 1.0f / myFftSizeOut;
    }

    myWindow = (float*)memalign(16, myFftSizeIn * sizeof(float));
    for (size_t i = 0; i < myFftSizeIn; ++i) {
        myWindow[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (myFftSizeIn - 1)));
        PDEBUG("Window[%zu] = %f\n", i, myWindow[i]);
    }

#if USE_FFTW
    myFftIn = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn);
    myFront = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn);
    myFftPlan1 = fftwf_plan_dft_1d(myFftSizeIn,
            myFftIn, myFront,
            FFTW_FORWARD, FFTW_MEASURE);

    myBack = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut);
    myFftOut = (FFT_TYPE*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut);
    myFftPlan2 = fftwf_plan_dft_1d(myFftSizeOut,
            myBack, myFftOut,
            FFTW_BACKWARD, FFTW_MEASURE);

    myBufferIn = (complexf*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeIn / 2);
    myBufferOut = (complexf*)fftwf_malloc(sizeof(FFT_TYPE) * myFftSizeOut / 2);
#else
    myFftIn = (FFT_TYPE*)memalign(16, myFftSizeIn * sizeof(FFT_TYPE));
    myFftOut = (FFT_TYPE*)memalign(16, myFftSizeOut * sizeof(FFT_TYPE));
    myBufferIn = (complexf*)memalign(16, myFftSizeIn / 2 * sizeof(FFT_TYPE));
    myBufferOut = (complexf*)memalign(16, myFftSizeOut / 2 * sizeof(FFT_TYPE));
    myFront = (FFT_TYPE*)memalign(16, myFftSizeIn * sizeof(FFT_TYPE));
    myBack = (FFT_TYPE*)memalign(16, myFftSizeOut * sizeof(FFT_TYPE));
    myFftPlan1 = kiss_fft_alloc(myFftSizeIn, 0, NULL, NULL);
    myFftPlan2 = kiss_fft_alloc(myFftSizeOut, 1, NULL, NULL);
#endif

    memset(myBufferIn, 0, myFftSizeIn / 2 * sizeof(FFT_TYPE));
    memset(myBufferOut, 0, myFftSizeOut / 2 * sizeof(FFT_TYPE));
}


Resampler::~Resampler()
{
    PDEBUG("Resampler::~Resampler() @ %p\n", this);

#if USE_FFTW
    if (myFftPlan1 != NULL) { fftwf_free(myFftPlan1); }
    if (myFftPlan2 != NULL) { fftwf_free(myFftPlan2); }
    if (myFftIn != NULL) { fftwf_free(myFftIn); }
    if (myFftOut != NULL) { fftwf_free(myFftOut); }
    if (myBufferIn != NULL) { fftwf_free(myBufferIn); }
    if (myBufferOut != NULL) { fftwf_free(myBufferOut); }
    if (myFront != NULL) { fftwf_free(myFront); }
    if (myBack != NULL) { fftwf_free(myBack); }
    if (myWindow != NULL) { fftwf_free(myWindow); }
    fftwf_destroy_plan(myFftPlan1);
    fftwf_destroy_plan(myFftPlan2);
#else
    if (myFftPlan1 != NULL) { free(myFftPlan1); }
    if (myFftPlan2 != NULL) { free(myFftPlan2); }
    if (myFftIn != NULL) { free(myFftIn); }
    if (myFftOut != NULL) { free(myFftOut); }
    if (myBufferIn != NULL) { free(myBufferIn); }
    if (myBufferOut != NULL) { free(myBufferOut); }
    if (myFront != NULL) { free(myFront); }
    if (myBack != NULL) { free(myBack); }
    if (myWindow != NULL) { free(myWindow); }
    kiss_fft_cleanup();
#endif
}


int Resampler::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("Resampler::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength() * L / M);

    FFT_TYPE* in = reinterpret_cast<FFT_TYPE*>(dataIn->getData());
    FFT_TYPE* out = reinterpret_cast<FFT_TYPE*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);

#if defined(USE_SIMD) && !USE_FFTW
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    typedef struct {
        float r[4];
        float i[4];
    } fft_data;
    assert(sizeof(FFT_TYPE) == sizeof(fft_data));
    fft_data *fftDataIn = (fft_data*)myFftIn;
    fft_data *fftDataOut = (fft_data*)myFftOut;
    complexf *cplxIn = (complexf*)in;
    complexf *cplxOut = (complexf*)out;
    for (size_t i = 0, j = 0; i < sizeIn; ) {
        for (int k = 0; k < 4; ++k) {
            if (i < sizeIn) {
                for (size_t l = 0; l < myFftSizeIn / 2; ++l) {
                    fftDataIn[l].r[k] = myBufferIn[l].real();
                    fftDataIn[l].i[k] = myBufferIn[l].imag();
                    fftDataIn[myFftSizeIn / 2 + l].r[k] = cplxIn[i + l].real();
                    fftDataIn[myFftSizeIn / 2 + l].i[k] = cplxIn[i + l].imag();
                }
                memcpy(myBufferIn, cplxIn + i, myFftSizeIn / 2 * sizeof(complexf));
                i += myFftSizeIn / 2;
            } else {
                for (size_t l = 0; l < myFftSizeIn; ++l) {
                    fftDataIn[l].r[k] = 0.0f;
                    fftDataIn[l].i[k] = 0.0f;
                }
            }
        }
        for (size_t k = 0; k < myFftSizeIn; ++ k) {
            FFT_REAL(myFftIn[k]) = _mm_mul_ps(FFT_REAL(myFftIn[k]), _mm_set_ps1(myWindow[k]));
            FFT_IMAG(myFftIn[k]) = _mm_mul_ps(FFT_IMAG(myFftIn[k]), _mm_set_ps1(myWindow[k]));
        }

        kiss_fft(myFftPlan1, myFftIn, myFront);

        if (myFftSizeOut > myFftSizeIn) {
            memset(myBack, 0, myFftSizeOut * sizeof(FFT_TYPE));
            memcpy(myBack, myFront, myFftSizeIn / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut - (myFftSizeIn / 2)],
                    &myFront[myFftSizeIn / 2],
                    myFftSizeIn / 2 * sizeof(FFT_TYPE));
            // Copy input Fs
            FFT_REAL(myBack[myFftSizeIn / 2]) =
                FFT_REAL(myFront[myFftSizeIn / 2]);
            FFT_IMAG(myBack[myFftSizeIn / 2]) =
                FFT_IMAG(myFront[myFftSizeIn / 2]);
        } else {
            memcpy(myBack, myFront, myFftSizeOut / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut / 2],
                    &myFront[myFftSizeIn - (myFftSizeOut / 2)],
                    myFftSizeOut / 2 * sizeof(FFT_TYPE));
            // Average output Fs from input
            FFT_REAL(myBack[myFftSizeOut / 2]) =
                _mm_add_ps(FFT_REAL(myBack[myFftSizeOut / 2]),
                        FFT_REAL(myFront[myFftSizeOut / 2]));
            FFT_IMAG(myBack[myFftSizeOut / 2]) =
                _mm_add_ps(FFT_IMAG(myBack[myFftSizeOut / 2]),
                        FFT_IMAG(myFront[myFftSizeOut / 2]));
            FFT_REAL(myBack[myFftSizeOut / 2]) =
                _mm_mul_ps(FFT_REAL(myBack[myFftSizeOut / 2]), _mm_set_ps1(0.5f));
            FFT_IMAG(myBack[myFftSizeOut / 2]) =
                _mm_mul_ps(FFT_IMAG(myBack[myFftSizeOut / 2]), _mm_set_ps1(0.5f));
        }
        for (size_t k = 0; k < myFftSizeOut; ++k) {
            FFT_REAL(myBack[k]) = _mm_mul_ps(FFT_REAL(myBack[k]), _mm_set_ps1(myFactor));
            FFT_IMAG(myBack[k]) = _mm_mul_ps(FFT_IMAG(myBack[k]), _mm_set_ps1(myFactor));
        }

        kiss_fft(myFftPlan2, myBack, myFftOut);

        for (size_t k = 0; k < 4; ++k) {
            if (j < sizeOut) {
                for (size_t l = 0; l < myFftSizeOut / 2; ++l) {
                    cplxOut[j + l] = complexf(myBufferOut[l].real() + fftDataOut[l].r[k],
                                              myBufferOut[l].imag() + fftDataOut[l].i[k]);
                    myBufferOut[l] = complexf(fftDataOut[myFftSizeOut / 2 + l].r[k],
                                              fftDataOut[myFftSizeOut / 2 + l].i[k]);
                }
            }
            j += myFftSizeOut / 2;
        }
    }
#endif

#if USE_FFTW || (!defined(USE_SIMD))
    for (size_t i = 0, j = 0; i < sizeIn; i += myFftSizeIn / 2, j += myFftSizeOut / 2) {
        memcpy(myFftIn, myBufferIn, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        memcpy(myFftIn + (myFftSizeIn / 2), in + i, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        memcpy(myBufferIn, in + i, myFftSizeIn / 2 * sizeof(FFT_TYPE));
        for (size_t k = 0; k < myFftSizeIn; ++k) {
            FFT_REAL(myFftIn[k]) *= myWindow[k];
            FFT_IMAG(myFftIn[k]) *= myWindow[k];
        }

#if USE_FFTW
        fftwf_execute(myFftPlan1);
#else
        kiss_fft(myFftPlan1, myFftIn, myFront);
#endif

        if (myFftSizeOut > myFftSizeIn) {
            memset(myBack, 0, myFftSizeOut * sizeof(FFT_TYPE));
            memcpy(myBack, myFront, myFftSizeIn / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut - (myFftSizeIn / 2)],
                    &myFront[myFftSizeIn / 2],
                    myFftSizeIn / 2 * sizeof(FFT_TYPE));
            // Copy input Fs
            FFT_REAL(myBack[myFftSizeIn / 2]) =
                FFT_REAL(myFront[myFftSizeIn / 2]);
            FFT_IMAG(myBack[myFftSizeIn / 2]) =
                FFT_IMAG(myFront[myFftSizeIn / 2]);
        } else {
            memcpy(myBack, myFront, myFftSizeOut / 2 * sizeof(FFT_TYPE));
            memcpy(&myBack[myFftSizeOut / 2],
                    &myFront[myFftSizeIn - (myFftSizeOut / 2)],
                    myFftSizeOut / 2 * sizeof(FFT_TYPE));
            // Average output Fs from input
            FFT_REAL(myBack[myFftSizeOut / 2]) +=
                FFT_REAL(myFront[myFftSizeOut / 2]);
            FFT_IMAG(myBack[myFftSizeOut / 2]) +=
                FFT_IMAG(myFront[myFftSizeOut / 2]);
            FFT_REAL(myBack[myFftSizeOut / 2]) *= 0.5f;
            FFT_IMAG(myBack[myFftSizeOut / 2]) *= 0.5f;
        }
        for (size_t k = 0; k < myFftSizeOut; ++k) {
            FFT_REAL(myBack[k]) *= myFactor;
            FFT_IMAG(myBack[k]) *= myFactor;
        }

#if USE_FFTW
        fftwf_execute(myFftPlan2);
#else
        kiss_fft(myFftPlan2, myBack, myFftOut);
#endif

        for (size_t k = 0; k < myFftSizeOut / 2; ++k) {
            FFT_REAL(out[j + k]) = myBufferOut[k].real() + FFT_REAL(myFftOut[k]);
            FFT_IMAG(out[j + k]) = myBufferOut[k].imag() + FFT_IMAG(myFftOut[k]);
        }
        memcpy(myBufferOut, myFftOut + (myFftSizeOut / 2), (myFftSizeOut / 2) * sizeof(FFT_TYPE));
    }
#endif

    return 1;
}

