/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2022
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   It is an output driver using libiio targeting the PrecisionWave DEXTER board.
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

#include "output/Dexter.h"

#ifdef HAVE_DEXTER

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"

using namespace std;

namespace Output {

static constexpr size_t TRANSMISSION_FRAME_LEN = (2656 + 76 * 2552) * 4;

static string get_iio_error(int err)
{
    char dst[256];
    iio_strerror(err, dst, sizeof(dst));
    return string(dst);
}

Dexter::Dexter(SDRDeviceConfig& config) :
    SDRDevice(),
    m_conf(config)
{
    etiLog.level(info) << "Dexter:Creating the device";

    m_ctx = iio_create_local_context();
    if (!m_ctx) {
        throw std::runtime_error("Dexter: Unable to create iio scan context");
    }

    m_dexter_dsp_tx = iio_context_find_device(m_ctx, "dexter_dsp_tx");
    if (!m_dexter_dsp_tx) {
        throw std::runtime_error("Dexter: Unable to find dexter_dsp_tx iio device");
    }

    m_ad9957_tx0 = iio_context_find_device(m_ctx, "ad9957_tx0");
    if (!m_ad9957_tx0) {
        throw std::runtime_error("Dexter: Unable to find ad9957_tx0 iio device");
    }

    int r;

    // TODO make DC offset configurable and add to RC
    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "dc0", 0)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.dc0 = false: " << get_iio_error(r);
    }

    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "dc1", 0)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.dc1 = false: " << get_iio_error(r);
    }

    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "stream0_start_clks", 0)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = 0: " << get_iio_error(r);
    }

    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "gain0", m_conf.txgain)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = 0: " << get_iio_error(r);
    }

    //iio_device_attr_read_longlong(const struct iio_device *dev, const char *attr, long long *val);

    if (m_conf.sampleRate != 2048000) {
        throw std::runtime_error("Dexter: Only 2048000 samplerate supported");
    }

    tune(m_conf.lo_offset, m_conf.frequency);
    // TODO m_conf.frequency = m_dexter_dsp_tx->getFrequency(SOAPY_SDR_TX, 0);
    etiLog.level(info) << "Dexter:Actual frequency: " <<
        std::fixed << std::setprecision(3) <<
        m_conf.frequency / 1000.0 << " kHz.";

    // skip: Set bandwidth

    // skip: antenna

    // TODO: set H/W time

    // Prepare streams
    constexpr int CHANNEL_INDEX = 0;
    m_tx_channel = iio_device_get_channel(m_ad9957_tx0, CHANNEL_INDEX);
    if (m_tx_channel == nullptr) {
        throw std::runtime_error("Dexter: Cannot create IIO channel.");
    }

    iio_channel_enable(m_tx_channel);

    m_buffer = iio_device_create_buffer(m_ad9957_tx0, TRANSMISSION_FRAME_LEN/sizeof(int16_t), 0);
    if (!m_buffer) {
        throw std::runtime_error("Dexter: Cannot create IIO buffer.");
    }
}

Dexter::~Dexter()
{
    if (m_ctx) {
        if (m_dexter_dsp_tx) {
            iio_device_attr_write_longlong(m_dexter_dsp_tx, "gain0", 0);
        }

        if (m_buffer) {
            iio_buffer_destroy(m_buffer);
        }

        iio_context_destroy(m_ctx);
        m_ctx = nullptr;
    }
}

void Dexter::tune(double lo_offset, double frequency)
{
    // TODO lo_offset
    long long freq = m_conf.frequency - 204800000;
    int r = 0;

    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "frequency0", freq)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.frequency0 = " << freq << " : " << get_iio_error(r);
    }
}

double Dexter::get_tx_freq(void) const
{
    long long frequency = 0;
    int r = 0;

    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "frequency0", &frequency)) != 0) {
        etiLog.level(warn) << "Failed to read dexter_dsp_tx.frequency0 = " <<
            frequency << " : " << get_iio_error(r);
        return 0;
    }
    else {
        return frequency + 204800000;
    }
}

void Dexter::set_txgain(double txgain)
{
    int r = 0;
    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "gain0", txgain)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = 0: " << get_iio_error(r);
    }

    long long txgain_readback = 0;
    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "gain0", &txgain_readback)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = 0: " << get_iio_error(r);
    }
    else {
        m_conf.txgain = txgain_readback;
    }
}

double Dexter::get_txgain(void) const
{
    long long txgain_readback = 0;
    int r = 0;
    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "gain0", &txgain_readback)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = 0: " << get_iio_error(r);
    }
    return txgain_readback;
}

void Dexter::set_bandwidth(double bandwidth)
{
    // TODO
}

double Dexter::get_bandwidth(void) const
{
    return 0;
}

SDRDevice::RunStatistics Dexter::get_run_statistics(void) const
{
    RunStatistics rs;
    rs.num_underruns = underflows;
    rs.num_overruns = overflows;
    rs.num_late_packets = late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}


double Dexter::get_real_secs(void) const
{
    // TODO
    return 0;
}

void Dexter::set_rxgain(double rxgain)
{
    // TODO
}

double Dexter::get_rxgain(void) const
{
    // TODO
    return 0;
}

size_t Dexter::receive_frame(
        complexf *buf,
        size_t num_samples,
        struct frame_timestamp& ts,
        double timeout_secs)
{
    // TODO
    return 0;
}


bool Dexter::is_clk_source_ok() const
{
    // TODO
    return true;
}

const char* Dexter::device_name(void) const
{
    return "Dexter";
}

double Dexter::get_temperature(void) const
{
    // TODO
    // XADC contains temperature, but value is weird
    return std::numeric_limits<double>::quiet_NaN();
}

void Dexter::transmit_frame(const struct FrameData& frame)
{
    long long int timeNs = frame.ts.get_ns();
    const bool has_time_spec = (m_conf.enableSync and frame.ts.timestamp_valid);

    if (frame.buf.size() != TRANSMISSION_FRAME_LEN) {
        etiLog.level(debug) << "Dexter::transmit_frame Expected " <<
            TRANSMISSION_FRAME_LEN << " got " << frame.buf.size();
        throw std::runtime_error("Dexter: invalid buffer size");
    }

    // DabMod::launch_modulator ensures we get int16_t IQ here
    //const size_t num_samples = frame.buf.size() / (2*sizeof(int16_t));
    //const int16_t *buf = reinterpret_cast<const int16_t*>(frame.buf.data());

    memcpy(iio_buffer_start(m_buffer), frame.buf.data(), frame.buf.size());
    ssize_t pushed = iio_buffer_push(m_buffer);
    if (pushed < 0) {
        etiLog.level(error) << "Dexter: failed to push buffer " << get_iio_error(pushed);
    }

    num_frames_modulated++;
    // TODO overflows, late_packets

    long long attr_value = 0;
    int r = 0;

    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "buffer_underflows0", &attr_value)) == 0) {
        fprintf(stderr, "buffer_underflows0 %lld\n", attr_value);
        underflows = attr_value;
    }
}

} // namespace Output

#endif // HAVE_DEXTER


