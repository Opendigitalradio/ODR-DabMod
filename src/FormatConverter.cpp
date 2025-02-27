/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

    This flowgraph block converts complexf to signed integer.
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

#include "FormatConverter.h"
#include "PcDebug.h"
#include "Log.h"

#include <stdexcept>
#include <cstring>
#include <assert.h>
#include <sys/types.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

FormatConverter::FormatConverter(bool input_is_complexfix_wide, const std::string& format_out) :
    ModCodec(),
    m_input_complexfix_wide(input_is_complexfix_wide),
    m_format_out(format_out)
{ }

FormatConverter::~FormatConverter()
{
    if (
#if defined(__ARM_NEON)
    not m_input_complexfix_wide
#else
    true
#endif
    ) {
        etiLog.level(debug) << "FormatConverter: " <<
            m_num_clipped_samples.load() << " clipped";
    }
}


/* Expect the input samples to be in the correct range for the required format */
int FormatConverter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("FormatConverter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    size_t num_clipped_samples = 0;

    if (m_input_complexfix_wide) {
        size_t sizeIn = dataIn->getLength() / sizeof(int32_t);
        if (m_format_out == "s16") {
            dataOut->setLength(sizeIn * sizeof(int16_t));
            const int32_t *in = reinterpret_cast<int32_t*>(dataIn->getData());
            int16_t* out = reinterpret_cast<int16_t*>(dataOut->getData());

            constexpr int shift = 6;

#if defined(__ARM_NEON)
            if (sizeIn % 4 != 0) {
                throw std::logic_error("Unexpected length not multiple of 4");
            }

            for (size_t i = 0; i < sizeIn; i += 4) {
                int32x4_t input_vec = vld1q_s32(&in[i]);
                // Apply shift right, saturate on conversion to int16_t
                int16x4_t output_vec = vqshrn_n_s32(input_vec, shift);
                vst1_s16(&out[i], output_vec);
            }
#else
            for (size_t i = 0; i < sizeIn; i++) {
                const int32_t val = in[i] >> shift;
                if (val < INT16_MIN) {
                    out[i] = INT16_MIN;
                    num_clipped_samples++;
                }
                else if (val > INT16_MAX) {
                    out[i] = INT16_MAX;
                    num_clipped_samples++;
                }
                else {
                    out[i] = val;
                }
            }
#endif
        }
        else {
            throw std::runtime_error("FormatConverter: Invalid fix format " + m_format_out);
        }
    }
    else {
        size_t sizeIn = dataIn->getLength() / sizeof(float);
        const float* in = reinterpret_cast<float*>(dataIn->getData());

        if (m_format_out == "s16") {
            dataOut->setLength(sizeIn * sizeof(int16_t));
            int16_t* out = reinterpret_cast<int16_t*>(dataOut->getData());

            for (size_t i = 0; i < sizeIn; i++) {
                if (in[i] < INT16_MIN) {
                    out[i] = INT16_MIN;
                    num_clipped_samples++;
                }
                else if (in[i] > INT16_MAX) {
                    out[i] = INT16_MAX;
                    num_clipped_samples++;
                }
                else {
                    out[i] = in[i];
                }
            }
        }
        else if (m_format_out == "u8") {
            dataOut->setLength(sizeIn * sizeof(int8_t));
            uint8_t* out = reinterpret_cast<uint8_t*>(dataOut->getData());

            for (size_t i = 0; i < sizeIn; i++) {
                const auto samp = in[i] + 128.0f;
                if (samp < 0) {
                    out[i] = 0;
                    num_clipped_samples++;
                }
                else if (samp > UINT8_MAX) {
                    out[i] = UINT8_MAX;
                    num_clipped_samples++;
                }
                else {
                    out[i] = samp;
                }

            }
        }
        else if (m_format_out == "s8") {
            dataOut->setLength(sizeIn * sizeof(int8_t));
            int8_t* out = reinterpret_cast<int8_t*>(dataOut->getData());

            for (size_t i = 0; i < sizeIn; i++) {
                if (in[i] < INT8_MIN) {
                    out[i] = INT8_MIN;
                    num_clipped_samples++;
                }
                else if (in[i] > INT8_MAX) {
                    out[i] = INT8_MAX;
                    num_clipped_samples++;
                }
                else {
                    out[i] = in[i];
                }
            }
        }
        else {
            throw std::runtime_error("FormatConverter: Invalid format " + m_format_out);
        }
    }

    m_num_clipped_samples.store(num_clipped_samples);
    return dataOut->getLength();
}

const char* FormatConverter::name()
{
    return "FormatConverter";
}

size_t FormatConverter::get_num_clipped_samples() const
{
    return m_num_clipped_samples.load();
}


size_t FormatConverter::get_format_size(const std::string& format)
{
    // Returns 2*sizeof(SAMPLE_TYPE) because we have I + Q
    if (format == "s16") {
        return 4;
    }
    else if (format == "u8") {
        return 2;
    }
    else if (format == "s8") {
        return 2;
    }
    else {
        throw std::runtime_error("FormatConverter: Invalid format " + format);
    }
}
