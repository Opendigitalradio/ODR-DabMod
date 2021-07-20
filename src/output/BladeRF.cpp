/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li
   
   Copyright (C) 2021
   Steven Rossel, steven.rossel@bluewin.ch

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

#ifdef HAVE_BLADERF

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"

using namespace std;

namespace Output
{

BladeRF::BladeRF(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{

    etiLog.level(info) << "BladeRF:Creating the device with: " << m_conf.device;
    
    struct bladerf_devinfo devinfo;

    bladerf_init_devinfo(&devinfo); // this function does not return a status
    
    int status = bladerf_open_with_devinfo(&m_device, &devinfo);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot open BladeRF output device");
    }

    if (m_conf.refclk_src == "pps")
    {
        status = bladerf_set_vctcxo_tamer_mode(m_device, BLADERF_VCTCXO_TAMER_1_PPS);
        if (status < 0)
        {
            etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
            throw runtime_error("Cannot set BladeRF refclk to pps");
        }
    }
    else if (m_conf.refclk_src == "10mhz")
    {
        status = bladerf_set_vctcxo_tamer_mode(m_device, BLADERF_VCTCXO_TAMER_10_MHZ);
        if (status < 0)
        {
            etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
            throw runtime_error("Cannot set BladeRF refclk to 10 MHz");
        }
    }
    else
    {
        status = bladerf_set_vctcxo_tamer_mode(m_device, BLADERF_VCTCXO_TAMER_DISABLED);
        if (status < 0)
        {
            etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
            throw runtime_error("Cannot disable BladeRF refclk");
        }
    }
    
    status = bladerf_set_sample_rate(m_device, m_channel, (bladerf_sample_rate)m_conf.sampleRate, NULL);
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

    tune(m_conf.lo_offset, m_conf.frequency);

    bladerf_frequency cur_frequency = 0;

    status = bladerf_get_frequency(m_device, m_channel, &cur_frequency);
    if(status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot get BladeRF frequency");
    }
    etiLog.level(info) << "BladeRF:Actual frequency: " << fixed << setprecision(3) << cur_frequency / 1000.0 << " kHz.";

    status = bladerf_set_gain(m_device, m_channel, (bladerf_gain)m_conf.txgain); // gain in [dB]
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF gain");
    }

    bladerf_bandwidth cur_bandwidth = 0;
    status = bladerf_set_bandwidth(m_device, m_channel, (bladerf_bandwidth)m_conf.bandwidth, &cur_bandwidth);
    if (status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot set BladeRF bandwidth");
    }

    /* ---------------------------- Streaming Config ---------------------------- */
    const unsigned int num_buffers = 16;   // Number of buffers to use in the underlying data stream
    const unsigned int buffer_size = 8192; // "to hold 2048 samples for one channel, a buffer must be at least 8192 bytes large"
    const unsigned int num_transfers = 8;  // active USB transfers
    const unsigned int timeout_ms = 3500;
    /* Configure the device's x1 TX (SISO) channel for use with the
    * synchronous interface. SC16 Q11 samples *without* metadata are used. */
    status = bladerf_sync_config(m_device, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11, num_buffers,
                                    buffer_size, num_transfers, timeout_ms);
    if (status != 0) {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot setup BladeRF stream");
    }

    status = bladerf_enable_module(m_device, m_channel, true);
    if(status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot enable BladeRF channel");
    }

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

    if (lo_offset != 0)
    {
        etiLog.level(info) << "lo_offset cannot be set at "<< std::to_string(lo_offset) << " with BladeRF output, has to be 0"
                           << "\nlo_offset is now set to 0";
        m_conf.lo_offset = 0;
    }

    status = bladerf_set_frequency(m_device, m_channel, (bladerf_frequency)m_conf.frequency);
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
    status = bladerf_set_gain(m_device, m_channel, (bladerf_gain)m_conf.txgain); // gain in [dB]
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
    bladerf_set_bandwidth(m_device, m_channel, (bladerf_bandwidth)m_conf.bandwidth, NULL);
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


void BladeRF::transmit_frame(const struct FrameData &frame)
{
    // The frame buffer contains bytes representing SC16 samples
    const int16_t* data_in = reinterpret_cast<const int16_t*>(&frame.buf[0]);
    const size_t num_samples = frame.buf.size() / (2*sizeof(int16_t));

    int status;
    status = bladerf_sync_tx(m_device, data_in, num_samples, NULL, 1000);
    if(status < 0)
    {
        etiLog.level(error) << "Error making BladeRF device: %s " << bladerf_strerror(status);
        throw runtime_error("Cannot transmit TX samples");
    }
}
} // namespace Output

#endif // HAVE_BLADERF
