/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Evariste F5OEO, evaristec@gmail.com

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver using the LimeSDR library.
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

#include "output/Lime.h"

#ifdef HAVE_LIMESDR

//#define LIMEDEBUG
#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"
#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif
using namespace std;

namespace Output
{

static constexpr size_t FRAMES_MAX_SIZE = 2;
static constexpr size_t FRAME_LENGTH = 196608; // at native sample rate!

#ifdef __ARM_NEON__
void conv_s16_from_float(unsigned n, const float *a, short *b)
{
    unsigned i;

    const float32x4_t plusone4 = vdupq_n_f32(1.0f);
    const float32x4_t minusone4 = vdupq_n_f32(-1.0f);
    const float32x4_t half4 = vdupq_n_f32(0.5f);
    const float32x4_t scale4 = vdupq_n_f32(32767.0f);
    const uint32x4_t mask4 = vdupq_n_u32(0x80000000);

    for (i = 0; i < n / 4; i++)
    {
        float32x4_t v4 = ((float32x4_t *)a)[i];
        v4 = vmulq_f32(vmaxq_f32(vminq_f32(v4, plusone4), minusone4), scale4);

        const float32x4_t w4 = vreinterpretq_f32_u32(vorrq_u32(vandq_u32(
                                                                   vreinterpretq_u32_f32(v4), mask4),
                                                               vreinterpretq_u32_f32(half4)));

        ((int16x4_t *)b)[i] = vmovn_s32(vcvtq_s32_f32(vaddq_f32(v4, w4)));
    }
}
#else
void conv_s16_from_float(unsigned n, const float *a, short *b)
{
    unsigned i;

    for (i = 0; i < n; i++)
    {
        b[i] = (short)(a[i] * 32767.0f);
    }
}
#endif

Lime::Lime(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{
    m_interpolate = m_conf.upsample;

    etiLog.level(info) << "Lime:Creating the device with: " << m_conf.device;

    const int device_count = LMS_GetDeviceList(nullptr);
    if (device_count < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot find LimeSDR output device");
    }

    lms_info_str_t device_list[device_count];
    if (LMS_GetDeviceList(device_list) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot find LimeSDR output device");
    }

    size_t device_i = 0; // If several cards, need to get device by configuration
    if (LMS_Open(&m_device, device_list[device_i], nullptr) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot open LimeSDR output device");
    }

    if (LMS_Reset(m_device) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot reset LimeSDR output device");
    }

    if (LMS_Init(m_device) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot init LimeSDR output device");
    }

    if (m_conf.masterClockRate != 0)
    {
        if (LMS_SetClockFreq(m_device, LMS_CLOCK_CGEN, m_conf.masterClockRate) < 0)
        {
            etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
            throw runtime_error("Cannot set master clock rate (CGEN) for LimeSDR output device");
        }

        float_type masterClockRate = 0;
        if (LMS_GetClockFreq(m_device, LMS_CLOCK_CGEN, &masterClockRate) < 0)
        {
            etiLog.level(error) << "Error reading CGEN clock LimeSDR device: %s " << LMS_GetLastErrorMessage();
        }
        else
        {
            etiLog.level(info) << "LimeSDR master clock rate set to " << fixed << setprecision(4) << masterClockRate;
        }
    }

    if (LMS_EnableChannel(m_device, LMS_CH_TX, m_channel, true) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot enable channel for LimeSDR output device");
    }

    if (LMS_SetSampleRate(m_device, m_conf.sampleRate * m_interpolate, 0) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set sample rate for LimeSDR output device");
    }
    float_type host_sample_rate = 0.0;

    if (LMS_GetSampleRate(m_device, LMS_CH_TX, m_channel, &host_sample_rate, NULL) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot get samplerate for LimeSDR output device");
    }
    etiLog.level(info) << "LimeSDR sample rate set to " << fixed << setprecision(4) << host_sample_rate / 1000.0 << " kHz";

    tune(m_conf.lo_offset, m_conf.frequency);

    float_type cur_frequency = 0.0;

    if (LMS_GetLOFrequency(m_device, LMS_CH_TX, m_channel, &cur_frequency) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot get frequency for LimeSDR output device");
    }
    etiLog.level(info) << "LimeSDR:Actual frequency: " << fixed << setprecision(3) << cur_frequency / 1000.0 << " kHz.";

    if (LMS_SetNormalizedGain(m_device, LMS_CH_TX, m_channel, m_conf.txgain / 100.0) < 0)
    {
        //value 0..100 -> Normalize
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set TX gain for LimeSDR output device");
    }

    if (LMS_SetAntenna(m_device, LMS_CH_TX, m_channel, LMS_PATH_TX2) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set antenna for LimeSDR output device");
    }

    double bandwidth_calibrating = 2.5e6; // Minimal bandwidth
    if (LMS_Calibrate(m_device, LMS_CH_TX, m_channel, bandwidth_calibrating, 0) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot calibrate LimeSDR output device");
    }

    switch (m_interpolate)
    {
    case 1:
    {
        //design matlab
        static double coeff[61] = {
            -0.0008126748726, -0.0003874975955, 0.0007290032809, -0.0009636150789, 0.0007643355639,
            3.123887291e-05, -0.001263667713, 0.002418729011, -0.002785810735, 0.001787990681,
            0.0006407162873, -0.003821208142, 0.006409643684, -0.006850919221, 0.004091503099,
            0.00172403187, -0.008917749859, 0.01456955727, -0.01547530293, 0.009518089704,
            0.00304264226, -0.01893160492, 0.0322769247, -0.03613986075, 0.02477015182,
            0.0041426518, -0.04805115238, 0.09958232939, -0.1481673121, 0.1828524768,
            0.8045722842, 0.1828524768, -0.1481673121, 0.09958232939, -0.04805115238,
            0.0041426518, 0.02477015182, -0.03613986075, 0.0322769247, -0.01893160492,
            0.00304264226, 0.009518089704, -0.01547530293, 0.01456955727, -0.008917749859,
            0.00172403187, 0.004091503099, -0.006850919221, 0.006409643684, -0.003821208142,
            0.0006407162873, 0.001787990681, -0.002785810735, 0.002418729011, -0.001263667713,
            3.123887291e-05, 0.0007643355639, -0.0009636150789, 0.0007290032809, -0.0003874975955,
            -0.0008126748726};

        LMS_SetGFIRCoeff(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, coeff, 61);
    }
    break;
    
    default:
        throw runtime_error("Unsupported interpolate: " + to_string(m_interpolate));
    }

    if (m_conf.sampleRate != 2048000)
    {
        throw runtime_error("Lime output only supports native samplerate = 2048000");
        /* The buffer_size calculation below does not take into account resampling */
    }

    // Frame duration is 96ms
    size_t buffer_size = FRAME_LENGTH * m_interpolate * 10; // We take 10 Frame buffer size Fifo
    // Fifo seems to be round to multiple of SampleRate
    m_tx_stream.channel = m_channel;
    m_tx_stream.fifoSize = buffer_size;
    m_tx_stream.throughputVsLatency = 2.0; // Should be {0..1} but could be extended 
    m_tx_stream.isTx = LMS_CH_TX;
    m_tx_stream.dataFmt = lms_stream_t::LMS_FMT_I16;
    if (LMS_SetupStream(m_device, &m_tx_stream) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot setup TX stream for LimeSDR output device");
    }
    LMS_StartStream(&m_tx_stream);
    LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
}

Lime::~Lime()
{
    if (m_device != nullptr)
    {
        LMS_StopStream(&m_tx_stream);
        LMS_DestroyStream(m_device, &m_tx_stream);
        LMS_EnableChannel(m_device, LMS_CH_TX, m_channel, false);
        LMS_Close(m_device);
    }
}

void Lime::tune(double lo_offset, double frequency)
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    if (LMS_SetLOFrequency(m_device, LMS_CH_TX, m_channel, m_conf.frequency) < 0)
    {
        etiLog.level(error) << "Error setting LimeSDR TX frequency: %s " << LMS_GetLastErrorMessage();
    }
}

double Lime::get_tx_freq(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    float_type cur_frequency = 0.0;

    if (LMS_GetLOFrequency(m_device, LMS_CH_TX, m_channel, &cur_frequency) < 0)
    {
        etiLog.level(error) << "Error getting LimeSDR TX frequency: %s " << LMS_GetLastErrorMessage();
    }

    return cur_frequency;
}

void Lime::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device)
        throw runtime_error("Lime device not set up");

    if (LMS_SetNormalizedGain(m_device, LMS_CH_TX, m_channel, m_conf.txgain / 100.0) < 0)
    {
        etiLog.level(error) << "Error setting LimeSDR TX gain: %s " << LMS_GetLastErrorMessage();
    }
}

double Lime::get_txgain(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    float_type txgain = 0;
    if (LMS_GetNormalizedGain(m_device, LMS_CH_TX, m_channel, &txgain) < 0)
    {
        etiLog.level(error) << "Error getting LimeSDR TX gain: %s " << LMS_GetLastErrorMessage();
    }
    return txgain;
}

void Lime::set_bandwidth(double bandwidth)
{
    LMS_SetLPFBW(m_device, LMS_CH_TX, m_channel, bandwidth);
}

double Lime::get_bandwidth(void) const
{
    double bw;
    LMS_GetLPFBW(m_device, LMS_CH_TX, m_channel, &bw);
    return bw;
}

SDRDevice::RunStatistics Lime::get_run_statistics(void) const
{
    RunStatistics rs;
    rs.num_underruns = underflows;
    rs.num_overruns = overflows;
    rs.num_late_packets = late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}

double Lime::get_real_secs(void) const
{
    // TODO
    return 0.0;
}

void Lime::set_rxgain(double rxgain)
{
    // TODO
}

double Lime::get_rxgain(void) const
{
    // TODO
    return 0.0;
}

size_t Lime::receive_frame(
    complexf *buf,
    size_t num_samples,
    struct frame_timestamp &ts,
    double timeout_secs)
{
    // TODO
    return 0;
}

bool Lime::is_clk_source_ok() const
{
    // TODO
    return true;
}

const char *Lime::device_name(void) const
{
    return "Lime";
}

double Lime::get_temperature(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    float_type temp = numeric_limits<float_type>::quiet_NaN();
    if (LMS_GetChipTemperature(m_device, 0, &temp) < 0)
    {
        etiLog.level(error) << "Error getting LimeSDR temperature: %s " << LMS_GetLastErrorMessage();
    }
    return temp;
}

float Lime::get_fifo_fill_percent(void) const
{
    return m_last_fifo_fill_percent * 100;
}

void Lime::transmit_frame(const struct FrameData &frame)
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    // The frame buffer contains bytes representing FC32 samples
    const complexf *buf = reinterpret_cast<const complexf *>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);

    m_i16samples.resize(numSamples * 2);
    short *buffi16 = &m_i16samples[0];
    
    conv_s16_from_float(numSamples * 2, (const float *)buf, buffi16);
    if ((frame.buf.size() % sizeof(complexf)) != 0)
    {
        throw runtime_error("Lime: invalid buffer size");
    }

    lms_stream_status_t LimeStatus;
    LMS_GetStreamStatus(&m_tx_stream, &LimeStatus);
    overflows += LimeStatus.overrun;
    underflows += LimeStatus.underrun;
    late_packets += LimeStatus.droppedPackets;

#ifdef LIMEDEBUG
    etiLog.level(info) << LimeStatus.fifoFilledCount << "/" << LimeStatus.fifoSize << ":" << numSamples << "Rate" << LimeStatus.linkRate / (2 * 2.0);
    etiLog.level(info) << "overrun" << LimeStatus.overrun << "underun" << LimeStatus.underrun << "drop" << LimeStatus.droppedPackets;
#endif

    m_last_fifo_fill_percent.store((float)LimeStatus.fifoFilledCount / (float)LimeStatus.fifoSize);

    /*
    if(LimeStatus.fifoFilledCount>=5*FRAME_LENGTH*m_interpolate) // Start if FIFO is half full {
        if(not m_tx_stream_active) {
            etiLog.level(info) << "Fifo OK : Normal running";
            LMS_StartStream(&m_tx_stream);
            m_tx_stream_active = true;
        }
    }
*/

    ssize_t num_sent = 0;

    lms_stream_meta_t meta;
    meta.flushPartialPacket = true;
    meta.timestamp = 0;
    meta.waitForTimestamp = false;

    if (m_interpolate == 1)
    {
        num_sent = LMS_SendStream(&m_tx_stream, buffi16, numSamples, &meta, 1000);
    }

    

    if (num_sent == 0)
    {
        etiLog.level(info) << "Lime: zero samples sent" << num_sent;
    }
    else if (num_sent == -1)
    {
        etiLog.level(error) << "Error sending LimeSDR stream: %s " << LMS_GetLastErrorMessage();
    }

    num_frames_modulated++;
}

} // namespace Output

#endif // HAVE_LIMESDR
