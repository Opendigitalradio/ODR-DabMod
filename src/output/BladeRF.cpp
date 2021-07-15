/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
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

#include "BladeRF.h"

//FIXME: MUST be removed!!! (for editing only)
#define HAVE_BLADERF

#ifdef HAVE_BLADERF

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"
using namespace std;

/*
#include <cmath>
#include <iostream>
#include <assert.h>
#include <stdexcept>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
*/

using namespace std;

namespace Output
{

static constexpr size_t FRAMES_MAX_SIZE = 2;
static constexpr size_t FRAME_LENGTH = 196608; // at native sample rate!


BladeRF::BladeRF(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{
    m_interpolate = m_conf.upsample;

    etiLog.level(info) << "BladeRF:Creating the device with: " << m_conf.device;

    const int device_count = bladerf_get_device_list(nullptr); // line 251
    if (device_count < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: " << bladerf_strerror(device_count);
        throw runtime_error("Cannot find BladeRF output device");
    }

    struct bladerf_devinfo device_list[device_count];
    int status = bladerf_get_device_list((struct bladerf_devinfo**)&device_list);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot find BladeRF output device");
    }

    /* Function to init wildcard devinfos
    bladerf_init_devinfo(&device_list[device_count]);
    status = bladerf_open_with_devinfo(&m_device, &device_list[device_count]);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot open BladeRF output device");
    } 
    */

    size_t device_i = 0; // If several cards, need to get device by configuration
    status = bladerf_open(&m_device, device_list[device_i].serial);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot open BladeRF output device");
    }

    /* Need to find a reset-like function for bladerf
    if (LMS_Reset(m_device) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot reset LimeSDR output device");
    }
    */

    // The clock has to be 38.4 MHz on BladeRF xA4/9.
    // Only defining if it is internal or external oscillator here

    bladerf_clock_select clk_sel = CLOCK_SELECT_ONBOARD;
    if (m_conf.refclk_src == "internal")
    {
        clk_sel = CLOCK_SELECT_ONBOARD;
        status = bladerf_set_clock_select(m_device, clk_sel);
        if (status < 0)
        {
            etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
            throw runtime_error("Cannot set BladeRF oscillator to internal");
        }
    }
    else if (m_conf.refclk_src == "external")
    {
        clk_sel = CLOCK_SELECT_EXTERNAL;
        status = bladerf_set_clock_select(m_device, clk_sel);
        if (status < 0)
        {
            etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
            throw runtime_error("Cannot set BladeRF oscillator to external");
        }
    }
    else
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF oscillator to external nor internal");
    }
    
    status = bladerf_set_sample_rate(m_device, m_channel, bladerf_sample_rate(m_conf.sampleRate * m_interpolate), NULL);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF sample rate");
    }
    
    bladerf_sample_rate host_sample_rate = 0;
    status =  bladerf_get_sample_rate(m_device, m_channel, &host_sample_rate);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot get BladeRF sample rate");
    }
    etiLog.level(info) << "BladeRF sample rate set to " << std::to_string(host_sample_rate / 1000.0) << " kHz";

    tune(m_conf.lo_offset, m_conf.frequency); //lo_offset?

    bladerf_frequency cur_frequency = 0;

    status = bladerf_get_frequency(m_device, m_channel, &cur_frequency);
    if(status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot get BladeRF frequency");
    }
    etiLog.level(info) << "BladeRF:Actual frequency: " << fixed << setprecision(3) << cur_frequency / 1000.0 << " kHz.";

    status = bladerf_set_gain(m_device, m_channel, bladerf_gain(m_conf.txgain)); // gain in [dB]
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF gain");
    }

    // libbladerf does not provide a setting function for antennas
    /*
    if (LMS_SetAntenna(m_device, LMS_CH_TX, m_channel, LMS_PATH_TX2) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot set antenna for LimeSDR output device");
    }
    */

    bladerf_bandwidth cur_bandwidth = 0;
    status = bladerf_set_bandwidth(m_device, m_channel, bladerf_bandwidth(m_conf.bandwidth), &cur_bandwidth);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF bandwidth");
    }

    status = bladerf_enable_module(m_device, m_channel, true);
    if(status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot enable BladeRF channel");
    }

#if 0   //FIXME: Needed ???
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
        break;  
    }
    default:
        throw runtime_error("Unsupported interpolate: " + to_string(m_interpolate));
    }
#endif

    if (m_conf.sampleRate != 2048000)
    {
        throw runtime_error("BladeRF output only supports native samplerate = 2048000");
        /* The buffer_size calculation below does not take into account resampling */
    }

    // Frame duration is 96ms
    //size_t buffer_size = FRAME_LENGTH * m_interpolate * 10; // We take 10 Frame buffer size Fifo

    bladerf_stream_cb callback; // callback function
    status = bladerf_init_stream(&m_stream, m_device, callback, );



    if (LMS_SetupStream(m_device, &m_tx_stream) < 0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw runtime_error("Cannot setup TX stream for LimeSDR output device");
    }
    LMS_StartStream(&m_tx_stream);
    LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
}

BladeRF::~BladeRF()
{
    if (m_device != nullptr)
    {
        bladerf_deinit_stream(m_stream);
        bladerf_enable_module(m_device, m_channel, false);
        bladerf_close(m_device);
    }
}

void BladeRF::tune(double lo_offset, double frequency)
{
    int status;
    if (not m_device)
        throw runtime_error("BladeRF device not set up");

    status = bladerf_set_frequency(m_device, m_channel, bladerf_frequency(m_conf.frequency));
    if (status < 0)
    {
         etiLog.level(error) << "Error setting BladeRF TX frequency: %s " << bladerf_strerror(status);
    }
}

double BladeRF::get_tx_freq(void) const
{
    if (not m_device)
        throw runtime_error("Lime device not set up");

    int status;
    bladerf_frequency cur_frequency = 0;

    status = bladerf_get_frequency(m_device, m_channel, &cur_frequency);
    if (status < 0)
    {
        etiLog.level(error) << "Error getting BladeRF TX frequency: %s " << bladerf_strerror(status);
    }

    return double(cur_frequency);
}

void BladeRF::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device)
        throw runtime_error("Lime device not set up");

    int status;
    status = bladerf_set_gain(m_device, m_channel, bladerf_gain(m_conf.txgain)); // gain in [dB]
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
    }
}

double BladeRF::get_txgain(void) const
{
    if (not m_device)
        throw runtime_error("BladeRF device not set up");

    bladerf_gain txgain = 0;
    int status;

    status = bladerf_get_gain(m_device, m_channel, &txgain);

    if (status < 0)
    {
        etiLog.level(error) << "Error getting BladeRF TX gain: %s " << bladerf_strerror(status);
    }
    return double(txgain);
}

void BladeRF::set_bandwidth(double bandwidth)
{
    bladerf_set_bandwidth(m_device, m_channel, bladerf_bandwidth(m_conf.bandwidth), NULL);
}

double BladeRF::get_bandwidth(void) const
{
    bladerf_bandwidth bw;
    bladerf_get_bandwidth(m_device, m_channel, &bw);
    return double(bw);
}

SDRDevice::RunStatistics BladeRF::get_run_statistics(void) const
{
    RunStatistics rs;
    rs.num_underruns = underflows;
    rs.num_overruns = overflows;
    rs.num_late_packets = late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}

double BladeRF::get_real_secs(void) const
{
    // TODO
    return 0.0;
}

void BladeRF::set_rxgain(double rxgain)
{
    // TODO
}

double BladeRF::get_rxgain(void) const
{
    // TODO
    return 0.0;
}

size_t BladeRF::receive_frame(
    complexf *buf,
    size_t num_samples,
    struct frame_timestamp &ts,
    double timeout_secs)
{
    // TODO
    return 0;
}

bool BladeRF::is_clk_source_ok() const
{
    // TODO
    return true;
}

const char *BladeRF::device_name(void) const
{
    return "BladeRF";
}

double BladeRF::get_temperature(void) const
{
    if (not m_device)
        throw runtime_error("BladeRF device not set up");

    float temp = 0.0;

    int status;
    status = bladerf_get_rfic_temperature(m_device, &temp);
    if (status < 0)
    {
        etiLog.level(error) << "Error getting BladeRF temperature: %s " << bladerf_strerror(status);
    }

    return temp;
}

float BladeRF::get_fifo_fill_percent(void) const
{
    return m_last_fifo_fill_percent * 100;
}


void BladeRF::transmit_frame(const struct FrameData &frame)
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

#endif // HAVE_BLADERF
