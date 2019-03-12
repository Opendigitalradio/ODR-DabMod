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

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"

using namespace std;

namespace Output
{

static constexpr size_t FRAMES_MAX_SIZE = 2;
static constexpr size_t FRAME_LENGTH = 196608; // at native sample rate!

Lime::Lime(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{
    m_interpolate = m_conf.upsample;

    etiLog.level(info) << "Lime:Creating the device with: " << m_conf.device;

    const int device_count = LMS_GetDeviceList(nullptr);
    if (device_count < 0) {
        etiLog.level(error) << "Error making LimeSDR device: " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot find LimeSDR output device");
    }

    lms_info_str_t device_list[device_count];
    if (LMS_GetDeviceList(device_list) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot find LimeSDR output device");
    }

    size_t device_i = 0; // If several cards, need to get device by configuration
    if (LMS_Open(&m_device, device_list[device_i], nullptr) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot open LimeSDR output device");
    }

    if (LMS_Reset(m_device) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot reset LimeSDR output device");
    }

    if (LMS_Init(m_device) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot init LimeSDR output device");
    }

    if (m_conf.masterClockRate != 0) {
        if (LMS_SetClockFreq(m_device, LMS_CLOCK_CGEN, m_conf.masterClockRate) < 0) {
            etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
            throw runtime_error("Cannot set master clock rate (CGEN) for LimeSDR output device");
        }

        float_type masterClockRate = 0;
        if (LMS_GetClockFreq(m_device, LMS_CLOCK_CGEN, &masterClockRate) < 0) {
            etiLog.level(error) << "Error reading CGEN clock LimeSDR device: %s " << LMS_GetLastErrorMessage();
        }
        else {
            etiLog.level(info) << "LimeSDR master clock rate set to " << fixed << setprecision(4) <<
                masterClockRate;
        }
    }

    if (LMS_EnableChannel(m_device, LMS_CH_TX, m_channel, true) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot enable channel for LimeSDR output device");
    }

    if (LMS_SetSampleRate(m_device, m_conf.sampleRate * m_interpolate, 0) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set sample rate for LimeSDR output device");
    }
    float_type host_sample_rate = 0.0;

    if (LMS_GetSampleRate(m_device, LMS_CH_TX, m_channel, &host_sample_rate, NULL) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot get samplerate for LimeSDR output device");
    }
    etiLog.level(info) << "LimeSDR sample rate set to " << fixed << setprecision(4) <<
        host_sample_rate / 1000.0 << " kHz";

    tune(m_conf.lo_offset, m_conf.frequency);

    float_type cur_frequency = 0.0;

    if (LMS_GetLOFrequency(m_device, LMS_CH_TX, m_channel, &cur_frequency) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot get frequency for LimeSDR output device");
    }
    etiLog.level(info) << "LimeSDR:Actual frequency: " << fixed << setprecision(3) <<
        cur_frequency / 1000.0 << " kHz.";

    if (LMS_SetNormalizedGain(m_device, LMS_CH_TX, m_channel, m_conf.txgain / 100.0) < 0) {
        //value 0..100 -> Normalize
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set TX gain for LimeSDR output device");
    }

    if (LMS_SetAntenna(m_device, LMS_CH_TX, m_channel, LMS_PATH_TX2) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set antenna for LimeSDR output device");
    }

    double bandwidth_calibrating = 2.5e6; // Minimal bandwidth
    if (LMS_Calibrate(m_device, LMS_CH_TX, m_channel, bandwidth_calibrating, 0) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot calibrate LimeSDR output device");
    }

    switch (m_interpolate) {
        case 1:
            {
                static double coeff[] = {
                    -0.0014080960536375642, 0.0010270054917782545,
                    0.0002103941806126386, -0.0023147952742874622,
                    0.004256128799170256, -0.0038850826676934958,
                    -0.0006057845894247293, 0.008352266624569893,
                    -0.014639420434832573, 0.01275692880153656,
                    0.0012119393795728683, -0.02339744009077549,
                    0.04088031128048897, -0.03649924695491791,
                    -0.001745241112075746, 0.07178881019353867,
                    -0.15494878590106964, 0.22244733572006226,
                    0.7530255913734436, 0.22244733572006226,
                    -0.15494878590106964, 0.07178881019353867,
                    -0.001745241112075746, -0.03649924695491791,
                    0.04088031128048897, -0.02339744009077549,
                    0.0012119393795728683, 0.01275692880153656,
                    -0.014639420434832573, 0.008352266624569893,
                    -0.0006057845894247293, -0.0038850826676934958,
                    0.004256128799170256, -0.0023147952742874622,
                    0.0002103941806126386, 0.0010270054917782545,
                    -0.0014080960536375642};
                LMS_SetGFIRCoeff(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, coeff, 37);
                LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
            }
            break;
        case 2:
            {
                static double coeff[] = {0.0007009872933849692,
                    0.0006160094635561109, -0.0003868100175168365,
                    -0.0010892765130847692, -0.0003017585549969226,
                    0.0013388358056545258, 0.0014964848523959517,
                    -0.000810395460575819, -0.0028437587898224592,
                    -0.001026041223667562, 0.0033166243229061365,
                    0.004008698742836714, -0.0016114861937239766,
                    -0.006794447544962168, -0.0029077117796987295,
                    0.0070640090852975845, 0.009203733876347542,
                    -0.002605677582323551, -0.014204192906618118,
                    -0.007088471669703722, 0.013578214682638645,
                    0.019509244710206985, -0.0035577849484980106,
                    -0.028872046619653702, -0.016949573531746864,
                    0.02703845500946045, 0.045044951140880585,
                    -0.00423968443647027, -0.07416801154613495,
                    -0.05744718387722969, 0.09617383778095245,
                    0.30029231309890747, 0.39504382014274597,
                    0.30029231309890747, 0.09617383778095245,
                    -0.05744718387722969, -0.07416801154613495,
                    -0.00423968443647027, 0.045044951140880585,
                    0.02703845500946045, -0.016949573531746864,
                    -0.028872046619653702, -0.0035577849484980106,
                    0.019509244710206985, 0.013578214682638645,
                    -0.007088471669703722, -0.014204192906618118,
                    -0.002605677582323551, 0.009203733876347542,
                    0.0070640090852975845, -0.0029077117796987295,
                    -0.006794447544962168, -0.0016114861937239766,
                    0.004008698742836714, 0.0033166243229061365,
                    -0.001026041223667562, -0.0028437587898224592,
                    -0.000810395460575819, 0.0014964848523959517,
                    0.0013388358056545258, -0.0003017585549969226,
                    -0.0010892765130847692, -0.0003868100175168365,
                    0.0006160094635561109, 0.0007009872933849692};
                LMS_SetGFIRCoeff(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, coeff, 65);
                LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
            }
            break;
        default:
            throw runtime_error("Unsupported interpolate: " + to_string(m_interpolate));
    }

    if (m_conf.sampleRate != 2048000) {
        throw runtime_error("Lime output only supports native samplerate = 2048000");
        /* The buffer_size calculation below does not take into account resampling */
    }

    // Frame duration is 96ms
    size_t buffer_size = FRAME_LENGTH * m_interpolate * 10; // We take 10 Frame buffer size Fifo
    // Fifo seems to be round to multiple of SampleRate
    m_tx_stream.channel = m_channel;
    m_tx_stream.fifoSize = buffer_size;
    m_tx_stream.throughputVsLatency = 1.0;
    m_tx_stream.isTx = LMS_CH_TX;
    m_tx_stream.dataFmt = lms_stream_t::LMS_FMT_F32;
    if (LMS_SetupStream(m_device, &m_tx_stream) < 0) {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot setup TX stream for LimeSDR output device");
    }
    LMS_StartStream(&m_tx_stream);
    LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
}

Lime::~Lime()
{
    if (m_device != nullptr) {
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

    if (LMS_SetLOFrequency(m_device, LMS_CH_TX, m_channel, m_conf.frequency) < 0) {
        etiLog.level(error) << "Error setting LimeSDR TX frequency: %s " << LMS_GetLastErrorMessage();
    }
}

double Lime::get_tx_freq(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    float_type cur_frequency = 0.0;

    if (LMS_GetLOFrequency(m_device, LMS_CH_TX, m_channel, &cur_frequency) < 0) {
        etiLog.level(error) << "Error getting LimeSDR TX frequency: %s " << LMS_GetLastErrorMessage();
    }

    return cur_frequency;
}

void Lime::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device)
        throw runtime_error("Lime device not set up");

    if (LMS_SetNormalizedGain(m_device, LMS_CH_TX, m_channel, m_conf.txgain / 100.0) < 0) {
        etiLog.level(error) << "Error setting LimeSDR TX gain: %s " << LMS_GetLastErrorMessage();
    }
}

double Lime::get_txgain(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    float_type txgain = 0;
    if (LMS_GetNormalizedGain(m_device, LMS_CH_TX, m_channel, &txgain) < 0) {
        etiLog.level(error) << "Error getting LimeSDR TX gain: %s " << LMS_GetLastErrorMessage();
    }
    return txgain;
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
    if (LMS_GetChipTemperature(m_device, 0, &temp) < 0) {
        etiLog.level(error) << "Error getting LimeSDR temperature: %s " << LMS_GetLastErrorMessage();
    }
    return temp;
}

uint32_t Lime::get_fifo_fill_count(void) const
{
    return m_last_fifo_filled_count;
}

void Lime::transmit_frame(const struct FrameData &frame)
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    // The frame buffer contains bytes representing FC32 samples
    const complexf *buf = reinterpret_cast<const complexf *>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);
    if ((frame.buf.size() % sizeof(complexf)) != 0) {
        throw runtime_error("Lime: invalid buffer size");
    }

    lms_stream_status_t LimeStatus;
    LMS_GetStreamStatus(&m_tx_stream, &LimeStatus);
    overflows += LimeStatus.overrun;
    underflows += LimeStatus.underrun;
    late_packets += LimeStatus.droppedPackets;

#if LIMEDEBUG
    etiLog.level(info) << LimeStatus.fifoFilledCount << "/" << LimeStatus.fifoSize << ":" << numSamples << "Rate" << LimeStatus.linkRate / (2 * 2.0);
    etiLog.level(info) << "overrun" << LimeStatus.overrun << "underun" << LimeStatus.underrun << "drop" << LimeStatus.droppedPackets;
#endif

    m_last_fifo_filled_count.store(LimeStatus.fifoFilledCount);

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
    if (m_interpolate == 1) {
        num_sent = LMS_SendStream(&m_tx_stream, buf, numSamples, NULL, 1000);
    }

    if (m_interpolate > 1) { // We upsample (1 0 0 0), low pass filter is done by FIR
        interpolatebuf.resize(m_interpolate * numSamples);
        for (size_t i = 0; i < numSamples; i++) {
            interpolatebuf[i * m_interpolate] = buf[i];
            for (size_t j = 1; j < m_interpolate; j++)
                interpolatebuf[i * m_interpolate + j] = complexf(0, 0);
        }
        num_sent = LMS_SendStream(&m_tx_stream, interpolatebuf.data(), numSamples * m_interpolate, NULL, 1000);
    }

    if (num_sent == 0) {
        etiLog.level(info) << "Lime: zero samples sent" << num_sent;
    }
    else if (num_sent == -1) {
        etiLog.level(error) << "Error sending LimeSDR stream: %s " << LMS_GetLastErrorMessage();
    }

    num_frames_modulated++;
}

} // namespace Output

#endif // HAVE_LIMESDR
