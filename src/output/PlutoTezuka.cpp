/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2025
   Evariste F5OEO, evaristec@gmail.com

   http://opendigitalradio.org

DESCRIPTION:
   It is an output driver using the TezukaFirmware (https://github.com/F5OEO/tezuka_fw) .
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

#include "output/PlutoTezuka.h"

#ifdef HAVE_PLUTO_TEZUKA

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

PlutoTezuka::PlutoTezuka(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{
    
    etiLog.level(info) << "PlutoTezuka:Creating the device with: " << m_conf.device;
   
}

PlutoTezuka::~PlutoTezuka()
{
    if (m_device != nullptr)
    {
        
    }
}

void PlutoTezuka::tune(double lo_offset, double frequency)
{
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    
}

double PlutoTezuka::get_tx_freq(void) const
{
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    float_type cur_frequency = 0.0;

    return cur_frequency;
}

void PlutoTezuka::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    
}

double PlutoTezuka::get_txgain(void) const
{
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    float_type txgain = 0;
    return txgain;
}

void PlutoTezuka::set_bandwidth(double bandwidth)
{
    
}

double PlutoTezuka::get_bandwidth(void) const
{
    double bw;
    
    return bw;
}

SDRDevice::run_statistics_t Lime::get_run_statistics(void) const
{
    run_statistics_t rs;
    rs["underruns"].v = underflows;
    rs["overruns"].v = overflows;
    rs["dropped_packets"].v = dropped_packets;
    rs["frames"].v = num_frames_modulated;
    rs["fifo_fill"].v = m_last_fifo_fill_percent * 100;
    return rs;
}

double PlutoTezuka::get_real_secs(void) const
{
    // TODO
    return 0.0;
}

void PlutoTezuka::set_rxgain(double rxgain)
{
    // TODO
}

double PlutoTezuka::get_rxgain(void) const
{
    // TODO
    return 0.0;
}

size_t PlutoTezuka::receive_frame(
    complexf *buf,
    size_t num_samples,
    frame_timestamp &ts,
    double timeout_secs)
{
    // TODO
    return 0;
}

bool Lime::is_clk_source_ok()
{
    // TODO
    return true;
}

const char *PlutoTezuka::device_name(void) const
{
    return "PlutoTezuka";
}

std::optional<double> Lime::get_temperature(void) const
{
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    float_type temp = 0;
    
    return temp;
    
}


void PlutoTezuka::transmit_frame(struct FrameData&& frame)
{
    if (not m_device)
        throw runtime_error("PlutoTezuka device not set up");

    // The frame buffer contains bytes representing FC32 samples
    const complexf *buf = reinterpret_cast<const complexf *>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);

    m_i16samples.resize(numSamples * 2);
    short *buffi16 = &m_i16samples[0];
    
    conv_s16_from_float(numSamples * 2, (const float *)buf, buffi16);
    if ((frame.buf.size() % sizeof(complexf)) != 0)
    {
        throw runtime_error("PlutoTezuka: invalid buffer size");
    }

    
    ssize_t num_sent = 0;

    if (num_sent == 0)
    {
        etiLog.level(info) << "PlutoTezuka: zero samples sent" << num_sent;
    }
    else if (num_sent == -1)
    {
        etiLog.level(error) << "Error sending PlutoTezuka stream:  " ;
    }

    num_frames_modulated++;
}

} // namespace Output

#endif // HAVE_PLUTOTEZUKA
