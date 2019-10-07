/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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

#include "GainControl.h"
#include "PcDebug.h"

#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include <string>

#ifdef __SSE__
#  include <xmmintrin.h>
union __u128 {
    __m128 m;
    float f[4];
};
#endif


using namespace std;

static float var_variance;

GainControl::GainControl(size_t framesize,
                         GainMode& gainMode,
                         float& digGain,
                         float normalise,
                         float& varVariance) :
    PipelinedModCodec(),
    RemoteControllable("gain"),
#ifdef __SSE__
    m_frameSize(framesize * sizeof(complexf) / sizeof(__m128)),
#else // !__SSE__
    m_frameSize(framesize),
#endif
    m_digGain(digGain),
    m_normalise(normalise),
    m_var_variance_rc(varVariance),
    m_gainmode(gainMode),
    m_mutex()
{
    PDEBUG("GainControl::GainControl(%zu, %zu) @ %p\n", framesize, (size_t)m_gainmode, this);

    /* register the parameters that can be remote controlled */
    RC_ADD_PARAMETER(digital, "Digital Gain");
    RC_ADD_PARAMETER(mode, "Gainmode (fix|max|var)");
    RC_ADD_PARAMETER(var, "Variance setting for gainmode var (default: 4)");

    start_pipeline_thread();
}

GainControl::~GainControl()
{
    stop_pipeline_thread();
}

int GainControl::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("GainControl::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    dataOut->setLength(dataIn->getLength());

#ifdef __SSE__
    __m128 (*computeGain)(const __m128* in, size_t sizeIn);
#else
    float (*computeGain)(const complexf* in, size_t sizeIn);
#endif
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        var_variance = m_var_variance_rc;

        switch (m_gainmode) {
            case GainMode::GAIN_FIX:
                PDEBUG("Gain mode: fix\n");
                computeGain = computeGainFix;
                break;
            case GainMode::GAIN_MAX:
                PDEBUG("Gain mode: max\n");
                computeGain = computeGainMax;
                break;
            case GainMode::GAIN_VAR:
                PDEBUG("Gain mode: var\n");
                computeGain = computeGainVar;
                break;
            default:
                throw std::logic_error("Internal error: invalid gainmode");
        }
    }

    const float constantGain = m_normalise * m_digGain;

#ifdef __SSE__
    const __m128* in  = reinterpret_cast<const __m128*>(dataIn->getData());
    __m128* out       = reinterpret_cast<__m128*>(dataOut->getData());
    size_t  sizeIn    = dataIn->getLength() / sizeof(__m128);
    size_t  sizeOut   = dataOut->getLength() / sizeof(__m128);
    __u128  gain128;


    if ((sizeIn % m_frameSize) != 0) {
        PDEBUG("%zu != %zu\n", sizeIn, m_frameSize);
        throw std::runtime_error(
                "GainControl::process input size not valid!");
    }

    const auto constantGain4 = _mm_set1_ps(constantGain);

    for (size_t i = 0; i < sizeIn; i += m_frameSize) {
        gain128.m = computeGain(in, m_frameSize);
        gain128.m = _mm_mul_ps(gain128.m, constantGain4);

        PDEBUG("********** Gain: %10f **********\n", gain128.f[0]);

        ////////////////////////////////////////////////////////////////////////
        // Applying gain to output data
        ////////////////////////////////////////////////////////////////////////
        for (size_t sample = 0; sample < m_frameSize; ++sample) {
            out[sample] = _mm_mul_ps(in[sample], gain128.m);
        }

        in  += m_frameSize;
        out += m_frameSize;
    }
#else // !__SSE__
    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out  = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn  = dataIn->getLength() / sizeof(complexf);
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);
    float  gain;

    if ((sizeIn % m_frameSize) != 0) {
        PDEBUG("%zu != %zu\n", sizeIn, m_frameSize);
        throw std::runtime_error(
                "GainControl::process input size not valid!");
    }

    for (size_t i = 0; i < sizeIn; i += m_frameSize) {
        gain = constantGain * computeGain(in, m_frameSize);

        PDEBUG("********** Gain: %10f **********\n", gain);

        ////////////////////////////////////////////////////////////////////////
        // Applying gain to output data
        ////////////////////////////////////////////////////////////////////////
        for (size_t sample = 0; sample < m_frameSize; ++sample) {
            out[sample] = in[sample] * gain;
        }

        in  += m_frameSize;
        out += m_frameSize;
    }
#endif // __SSE__

    return sizeOut;
}


#ifdef __SSE__
__m128 GainControl::computeGainFix(const __m128* in, size_t sizeIn)
{
    return _mm_set1_ps(512.0f);
}

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
    }
    else {
        gain128.m = _mm_set1_ps(1.0f);
    }

    return gain128.m;
}

__m128 GainControl::computeGainVar(const __m128* in, size_t sizeIn)
{
    __u128 gain128;
    __u128 mean128;
    __u128 var128;
    __u128 tmp128;
    static const __m128 factor128 = _mm_set1_ps(0x7fff);

    mean128.m = _mm_setzero_ps();

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        __m128 delta128 = _mm_sub_ps(in[sample], mean128.m);
        __m128 i128 = _mm_set1_ps(sample + 1);
        __m128 q128 = _mm_div_ps(delta128, i128);
        mean128.m = _mm_add_ps(mean128.m, q128);

        /*
        tmp128.m = in[sample];
        printf("S %zu, %.2f+%.2fj\t",
                sample,
                tmp128.f[0], tmp128.f[1]);
        printf(": %.2f+%.2fj\n", mean128.f[0], mean128.f[1]);

        printf("S %zu, %.2f+%.2fj\t",
                sample,
                tmp128.f[2], tmp128.f[3]);
        printf(": %.2f+%.2fj\n", mean128.f[2], mean128.f[3]);
        */
    }

    // Merging average
    tmp128.m = _mm_shuffle_ps(mean128.m, mean128.m, _MM_SHUFFLE(1, 0, 3, 2));
    mean128.m = _mm_add_ps(mean128.m, tmp128.m);
    mean128.m = _mm_mul_ps(mean128.m, _mm_set1_ps(0.5f));
    PDEBUG("********** Mean:  %10f + %10fj %10f + %10fj **********\n",
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

        /*
        __u128 udiff128;  udiff128.m = diff128;
        __u128 udelta128; udelta128.m = delta128;
        for (int off=0; off<4; off+=2) {
            printf("S %zu, %.2f+%.2fj\t",
                    sample,
                    udiff128.f[off], udiff128.f[1+off]);
            printf(": %.2f+%.2fj\t", udelta128.f[off], udelta128.f[1+off]);
            printf(": %.2f+%.2fj\n", var128.f[off], var128.f[1+off]);
        }
        */

    }
    PDEBUG("********** Vars:  %10f + %10fj, %10f + %10fj **********\n",
            var128.f[0], var128.f[1], var128.f[2], var128.f[3]);

    // Merging standard deviations
    tmp128.m = _mm_shuffle_ps(var128.m, var128.m, _MM_SHUFFLE(1, 0, 3, 2));
    var128.m = _mm_add_ps(var128.m, tmp128.m);
    var128.m = _mm_mul_ps(var128.m, _mm_set1_ps(0.5f));
    var128.m = _mm_sqrt_ps(var128.m);
    PDEBUG("********** Var:   %10f + %10fj, %10f + %10fj **********\n",
            var128.f[0], var128.f[1], var128.f[2], var128.f[3]);
    var128.m = _mm_mul_ps(var128.m, _mm_set1_ps(var_variance));
    PDEBUG("********** 4*Var: %10f + %10fj, %10f + %10fj **********\n",
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

float GainControl::computeGainFix(const complexf* in, size_t sizeIn)
{
    return 512.0f;
}

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
    }
    else {
        gain = 1.0f;
    }

    return gain;
}

float GainControl::computeGainVar(const complexf* in, size_t sizeIn)
{
    complexf mean;

    /* The variance calculation is a bit strange, because we
     * emulate the exact same functionality as the SSE code,
     * which is the most used one.
     *
     * TODO: verify that this actually corresponds to the
     * gain mode suggested in EN 300 798 Clause 5.3 Numerical Range.
     */
    complexf var1;
    complexf var2;

    static const float factor = 0x7fff;

    mean = complexf(0.0f, 0.0f);

    for (size_t sample = 0; sample < sizeIn; ++sample) {
        const complexf delta = in[sample] - mean;
        const float i = sample + 1;
        const complexf q = delta / i;
        mean += q;

        /*
        printf("F %zu, %.2f+%.2fj\t",
                sample,
                in[sample].real(), in[sample].imag());

        printf(": %.2f+%.2fj\n", mean.real(), mean.imag());
        */
    }

    PDEBUG("********** Mean:  %10f + %10fj **********\n", mean.real(), mean.imag());

    ////////////////////////////////////////////////////////////////////////
    // Computing standard deviation
    ////////////////////////////////////////////////////////////////////////
    var1 = complexf(0.0f, 0.0f);
    var2 = complexf(0.0f, 0.0f);
    for (size_t sample = 0; sample < sizeIn; ++sample) {
        const complexf diff  = in[sample] - mean;
        complexf delta;
        complexf q;

        float    i = (sample/2) + 1;
        if (sample % 2 == 0) {
            delta = complexf(diff.real() * diff.real(),
                             diff.imag() * diff.imag()) - var1;
            q = delta / i;

            var1 += q;
        }
        else {
            delta = complexf(diff.real() * diff.real(),
                             diff.imag() * diff.imag()) - var2;
            q = delta / i;

            var2 += q;
        }

        /*
        printf("F %zu, %.2f+%.2fj\t",
                sample,
                diff.real(), diff.imag());

        printf(": %.2f+%.2fj\t", delta.real(), delta.imag());
        printf(": %.2f+%.2fj\t", var1.real(), var1.imag());
        printf(": %.2f+%.2fj\n", var2.real(), var2.imag());
        */
    }

    PDEBUG("********** Vars:  %10f + %10fj, %10f + %10fj **********\n",
                var1.real(), var1.imag(),
                var2.real(), var2.imag());

    // Merge standard deviations in the same way the SSE version does it
    complexf tmpvar = (var1 + var2) * 0.5f;
    complexf var(sqrt(tmpvar.real()), sqrt(tmpvar.imag()));
    PDEBUG("********** Var:   %10f + %10fj **********\n", var.real(), var.imag());

    var = var * var_variance;
    PDEBUG("********** 4*Var: %10f + %10fj **********\n", var.real(), var.imag());

    ////////////////////////////////////////////////////////////////////////////
    // Computing gain
    ////////////////////////////////////////////////////////////////////////////
    float gain = var.real();
    // gain = factor128 / max(real, imag)
    if (var.imag() > gain) {
        gain = var.imag();
    }

    // Ignore zero variance samples and apply no gain
    if ((int)gain == 0) {
        gain = 1.0f;
    }
    else {
        gain = factor / gain;
    }

    return gain;
}
#endif // !__SSE__

void GainControl::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "digital") {
        float new_factor;
        ss >> new_factor;
        m_digGain = new_factor;
    }
    else if (parameter == "mode") {
        string new_mode;
        ss >> new_mode;
        std::transform(new_mode.begin(), new_mode.end(), new_mode.begin(),
                [](const char c) { return std::tolower(c); } );

        GainMode m;
        if (new_mode == "fix") {
            m = GainMode::GAIN_FIX;
        }
        else if (new_mode == "max") {
            m = GainMode::GAIN_MAX;
        }
        else if (new_mode == "var") {
            m = GainMode::GAIN_VAR;
        }
        else {
            throw ParameterError("Gainmode " + new_mode + " unknown");
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_gainmode = m;
        }
    }
    else if (parameter == "var") {
        float newvar = 0;
        ss >> newvar;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_var_variance_rc = newvar;
        }
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
    }
}

const string GainControl::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "digital") {
        ss << std::fixed << m_digGain;
    }
    else if (parameter == "mode") {
        switch (m_gainmode) {
            case GainMode::GAIN_FIX:
                ss << "fix";
                break;
            case GainMode::GAIN_MAX:
                ss << "max";
                break;
            case GainMode::GAIN_VAR:
                ss << "var";
                break;
        }
    }
    else if (parameter == "var") {
        ss << std::fixed << m_var_variance_rc;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

