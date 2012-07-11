/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
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

#include "GainControl.h"
#include "PcDebug.h"
#include "kiss_fftsimd.h"

#include <stdio.h>
#include <stdexcept>


GainControl::GainControl(size_t framesize, GainMode mode, float factor) :
    ModCodec(ModFormat(framesize * sizeof(complexf)), ModFormat(framesize * sizeof(complexf))),
#ifdef __SSE__
    d_frameSize(framesize * sizeof(complexf) / sizeof(__m128)),
#else // !__SSE__
    d_frameSize(framesize),
#endif
    d_factor(factor)
{
    PDEBUG("GainControl::GainControl(%zu, %u) @ %p\n", framesize, mode, this);

    switch(mode) {
    case GAIN_FIX:
        PDEBUG("Gain mode: fix\n");
        computeGain = computeGainFix;
        break;
    case GAIN_MAX:
        PDEBUG("Gain mode: max\n");
        computeGain = computeGainMax;
        break;
    case GAIN_VAR:
        PDEBUG("Gain mode: var\n");
        computeGain = computeGainVar;
        break;
    default:
        throw std::runtime_error(
                "GainControl::GainControl invalid computation gain mode!");
    }
}


GainControl::~GainControl()
{
    PDEBUG("GainControl::~GainControl() @ %p\n", this);

}


int GainControl::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("GainControl::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength());

#ifdef __SSE__
    const __m128* in = reinterpret_cast<const __m128*>(dataIn->getData());
    __m128* out = reinterpret_cast<__m128*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(__m128);
    size_t sizeOut = dataOut->getLength() / sizeof(__m128);
    __u128 gain128;

    if ((sizeIn % d_frameSize) != 0) {
        PDEBUG("%zu != %zu\n", sizeIn, d_frameSize);
        throw std::runtime_error(
                "GainControl::process input size not valid!");
    }

    for (size_t i = 0; i < sizeIn; i += d_frameSize) {
        gain128.m = computeGain(in, d_frameSize);
        gain128.m = _mm_mul_ps(gain128.m, _mm_set1_ps(d_factor));

        PDEBUG("********** Gain: %10f **********\n", gain128.f[0]);

        ////////////////////////////////////////////////////////////////////////
        // Applying gain to output data
        ////////////////////////////////////////////////////////////////////////
        for (size_t sample = 0; sample < d_frameSize; ++sample) {
            out[sample] = _mm_mul_ps(in[sample], gain128.m);
        }

        in += d_frameSize;
        out += d_frameSize;
    }
#else // !__SSE__
    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);
    float gain;

    if ((sizeIn % d_frameSize) != 0) {
        PDEBUG("%zu != %zu\n", sizeIn, d_frameSize);
        throw std::runtime_error(
                "GainControl::process input size not valid!");
    }

    for (size_t i = 0; i < sizeIn; i += d_frameSize) {
        gain = myFactor * computeGain(in, d_frameSize);

        PDEBUG("********** Gain: %10f **********\n", gain);

        ////////////////////////////////////////////////////////////////////////
        // Applying gain to output data
        ////////////////////////////////////////////////////////////////////////
        for (size_t sample = 0; sample < d_frameSize; ++sample) {
            out[sample] = in[sample] * gain;
        }

        in += d_frameSize;
        out += d_frameSize;
    }
#endif // __SSE__
    
    return sizeOut;
}


#ifdef __SSE__
__m128 GainControl::computeGainFix(const __m128* in, size_t sizeIn)
{
    return _mm_set1_ps(512.0f);
}
#else // !__SSE__
float GainControl::computeGainFix(const complexf* in, size_t sizeIn)
{
    return 512.0f;
}
#endif // __SSE__


#ifdef __SSE__
__m128 GainControl::computeGainMax(const __m128* in, size_t sizeIn)
{
    __u128 gain128;
    __u128 min128;
    __u128 max128;
    __u128 tmp128;
    static const __m128 factor128 = _mm_set1_ps(0x7fff);

    ////////////////////////////////////////////////////////////////////////
    // Computing max, min and average
    ////////////////////////////////////////////////////////////////////////
    min128.m = _mm_set1_ps(__FLT_MAX__);
    max128.m = _mm_set1_ps(__FLT_MIN__);

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        min128.m = _mm_min_ps(in[sample], min128.m);
        max128.m = _mm_max_ps(in[sample], max128.m);
    }

    // Merging min
    tmp128.m = _mm_shuffle_ps(min128.m, min128.m, _MM_SHUFFLE(0, 1, 2, 3));
    min128.m = _mm_min_ps(min128.m, tmp128.m);
    tmp128.m = _mm_shuffle_ps(min128.m, min128.m, _MM_SHUFFLE(1, 0, 3, 2));
    min128.m = _mm_min_ps(min128.m, tmp128.m);
    PDEBUG("********** Min:   %10f  **********\n", min128.f[0]);

    // Merging max
    tmp128.m = _mm_shuffle_ps(max128.m, max128.m, _MM_SHUFFLE(0, 1, 2, 3));
    max128.m = _mm_max_ps(max128.m, tmp128.m);
    tmp128.m = _mm_shuffle_ps(max128.m, max128.m, _MM_SHUFFLE(1, 0, 3, 2));
    max128.m = _mm_max_ps(max128.m, tmp128.m);
    PDEBUG("********** Max:   %10f  **********\n", max128.f[0]);

    ////////////////////////////////////////////////////////////////////////////
    // Computing gain
    ////////////////////////////////////////////////////////////////////////////
    // max = max(-min, max)
    max128.m = _mm_max_ps(_mm_mul_ps(min128.m, _mm_set1_ps(-1.0f)), max128.m);
    // Detect NULL
    if ((int)max128.f[0] != 0) {
        gain128.m = _mm_div_ps(factor128, max128.m);
    } else {
        gain128.m = _mm_set1_ps(1.0f);
    }

    return gain128.m;
}
#else // !__SSE__
float GainControl::computeGainMax(const complexf* in, size_t sizeIn)
{
    float gain;
    float min;
    float max;
    static const float factor = 0x7fff;

    ////////////////////////////////////////////////////////////////////////
    // Computing max, min and average
    ////////////////////////////////////////////////////////////////////////
    min = __FLT_MAX__;
    max = __FLT_MIN__;

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        if (in[sample].real() < min) {
            min = in[sample].real();
        }
        if (in[sample].real() > max) {
            max = in[sample].real();
        }
        if (in[sample].imag() < min) {
            min = in[sample].imag();
        }
        if (in[sample].imag() > max) {
            max = in[sample].imag();
        }
    }

    PDEBUG("********** Min:  %10f **********\n", min);
    PDEBUG("********** Max:  %10f **********\n", max);

    ////////////////////////////////////////////////////////////////////////////
    // Computing gain
    ////////////////////////////////////////////////////////////////////////////
    // gain = factor128 / max(-min, max)
    min = -min;
    if (min > max) {
        max = min;
    }

    // Detect NULL
    if ((int)max != 0) {
        gain = factor / max;
    } else {
        gain = 1.0f;
    }

    return gain;
}
#endif // __SSE__


#ifdef __SSE__
__m128 GainControl::computeGainVar(const __m128* in, size_t sizeIn)
{
    __u128 gain128;
    __u128 mean128;
    __u128 var128;
    __u128 tmp128;
    static const __m128 factor128 = _mm_set1_ps(0x7fff);

    mean128.m = _mm_setzero_ps();
//    var128.m = _mm_setzero_ps();

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        __m128 delta128 = _mm_sub_ps(in[sample], mean128.m);
        __m128 i128 = _mm_set1_ps(sample + 1);
        __m128 q128 = _mm_div_ps(delta128, i128);
        mean128.m = _mm_add_ps(mean128.m, q128);
//        var128.m = _mm_add_ps(var128.m,
//                _mm_mul_ps(delta128, _mm_sub_ps(in[sample], mean128.m)));
    }

    // Merging average
    tmp128.m = _mm_shuffle_ps(mean128.m, mean128.m, _MM_SHUFFLE(1, 0, 3, 2));
    mean128.m = _mm_add_ps(mean128.m, tmp128.m);
    mean128.m = _mm_mul_ps(mean128.m, _mm_set1_ps(0.5f));
    PDEBUG("********** Mean:  %10f, %10f, %10f, %10f **********\n",
            mean128.f[0], mean128.f[1], mean128.f[2], mean128.f[3]);

    ////////////////////////////////////////////////////////////////////////
    // Computing standard deviation
    ////////////////////////////////////////////////////////////////////////
    var128.m = _mm_setzero_ps();
    for (size_t sample = 0; sample < sizeIn; ++sample) {
        __m128 diff128 = _mm_sub_ps(in[sample], mean128.m);
        __m128 delta128 = _mm_sub_ps(_mm_mul_ps(diff128, diff128), var128.m);
        __m128 i128 = _mm_set1_ps(sample + 1);
        __m128 q128 = _mm_div_ps(delta128, i128);
        var128.m = _mm_add_ps(var128.m, q128);
    }
    // Merging average
    tmp128.m = _mm_shuffle_ps(var128.m, var128.m, _MM_SHUFFLE(1, 0, 3, 2));
    var128.m = _mm_add_ps(var128.m, tmp128.m);
    var128.m = _mm_mul_ps(var128.m, _mm_set1_ps(0.5f));
    var128.m = _mm_sqrt_ps(var128.m);
    PDEBUG("********** Var:   %10f, %10f, %10f, %10f **********\n",
            var128.f[0], var128.f[1], var128.f[2], var128.f[3]);
    var128.m = _mm_mul_ps(var128.m, _mm_set1_ps(4.0f));
    PDEBUG("********** 4*Var: %10f, %10f, %10f, %10f **********\n",
            var128.f[0], var128.f[1], var128.f[2], var128.f[3]);

    ////////////////////////////////////////////////////////////////////////////
    // Computing gain
    ////////////////////////////////////////////////////////////////////////////
    // gain = factor128 / max(real, imag)
    // Detect NULL
    if ((int)var128.f[0] != 0) {
        gain128.m = _mm_div_ps(factor128,
                _mm_max_ps(var128.m,
                    _mm_shuffle_ps(var128.m, var128.m, _MM_SHUFFLE(2, 3, 0, 1))));
    } else {
        gain128.m = _mm_set1_ps(1.0f);
    }

    return gain128.m;
}
#else // !__SSE__
float GainControl::computeGainVar(const complexf* in, size_t sizeIn)
{
    float gain;
    complexf mean;
    complexf var;
    static const float factor = 0x7fff;

    mean = complexf(0.0f, 0.0f);

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        complexf delta = in[sample] - mean;
        float i = sample + 1;
        complexf q = delta / i;
        mean = mean + q;
    }

    PDEBUG("********** Mean:  %10f, %10f **********\n", mean.real(), mean.imag());

    ////////////////////////////////////////////////////////////////////////
    // Computing standard deviation
    ////////////////////////////////////////////////////////////////////////
    var = complexf(0.0f, 0.0f);
    for (size_t sample = 0; sample < sizeIn; ++sample) {
        complexf diff = in[sample] - mean;
        complexf delta = complexf(diff.real() * diff.real(), diff.imag() * diff.imag()) - var;
        float i = sample + 1;
        complexf q = delta / i;
        var = var + q;
    }
    PDEBUG("********** Var:   %10f, %10f **********\n", var.real(), var.imag());
    var = var * 4.0f;
    PDEBUG("********** 4*Var: %10f, %10f **********\n", var.real(), var.imag());

    ////////////////////////////////////////////////////////////////////////////
    // Computing gain
    ////////////////////////////////////////////////////////////////////////////
    // gain = factor128 / max(real, imag)
    if (var.imag() > var.real()) {
        var.real() = var.imag();
    }
    // Detect NULL
    if ((int)var.real() == 0) {
        gain = factor / var.real();
    } else {
        gain = 1.0f;
    }

    return gain;
}
#endif // __SSE__
