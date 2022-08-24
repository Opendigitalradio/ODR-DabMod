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

static constexpr uint64_t DSP_CLOCK = 2048000uLL * 80;

static constexpr size_t TRANSMISSION_FRAME_LEN = (2656 + 76 * 2552) * 4;
static constexpr size_t IIO_BUFFERS = 4;
static constexpr size_t IIO_BUFFER_LEN = TRANSMISSION_FRAME_LEN / IIO_BUFFERS;

static string get_iio_error(int err)
{
    char dst[256];
    iio_strerror(err, dst, sizeof(dst));
    return string(dst);
}

static void fill_time(struct timespec *t)
{
    if (clock_gettime(CLOCK_REALTIME, t) != 0) {
        throw std::runtime_error(string("Failed to retrieve CLOCK_REALTIME") + strerror(errno));
    }
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

    // get H/W time
    /* Procedure:
     * Wait 200ms after second change, fetch pps_clks attribute
     * idem at the next second, and check that pps_clks incremented by DSP_CLOCK
     * If ok, store the correspondence between current second change (measured in UTC clock time)
     * and the counter value at pps rising edge. */

    etiLog.level(info) << "Dexter: Waiting for second change...";

    struct timespec time_at_startup;
    fill_time(&time_at_startup);
    time_at_startup.tv_nsec = 0;

    struct timespec time_now;
    do {
        fill_time(&time_now);
        this_thread::sleep_for(chrono::milliseconds(1));
    } while (time_at_startup.tv_sec == time_now.tv_sec);
    this_thread::sleep_for(chrono::milliseconds(200));

    long long pps_clks = 0;
    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "pps_clks", &pps_clks)) != 0) {
        etiLog.level(error) << "Failed to get dexter_dsp_tx.pps_clks: " << get_iio_error(r);
        throw std::runtime_error("Dexter: Cannot read IIO attribute");
    }

    time_t tnow = time_now.tv_sec;
    etiLog.level(info) << "Dexter: pps_clks " << pps_clks << " at UTC " <<
        put_time(std::gmtime(&tnow), "%Y-%m-%d %H:%M:%S");

    time_at_startup.tv_sec = time_now.tv_sec;
    do {
        fill_time(&time_now);
        this_thread::sleep_for(chrono::milliseconds(1));
    } while (time_at_startup.tv_sec == time_now.tv_sec);
    this_thread::sleep_for(chrono::milliseconds(200));

    long long pps_clks2 = 0;
    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "pps_clks", &pps_clks2)) != 0) {
        etiLog.level(error) << "Failed to get dexter_dsp_tx.pps_clks: " << get_iio_error(r);
        throw std::runtime_error("Dexter: Cannot read IIO attribute");
    }
    tnow = time_now.tv_sec;
    etiLog.level(info) << "Dexter: pps_clks increased by " << pps_clks2 - pps_clks << " at UTC " <<
        put_time(std::gmtime(&tnow), "%Y-%m-%d %H:%M:%S");

    if ((uint64_t)pps_clks + DSP_CLOCK != (uint64_t)pps_clks2) {
        throw std::runtime_error("Dexter: Wrong increase of pps_clks, expected " + to_string(DSP_CLOCK));
    }
    m_utc_seconds_at_startup = time_now.tv_sec;
    m_clock_count_at_startup = pps_clks2;

    // Reset start_clks
    if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "stream0_start_clks", 0)) != 0) {
        etiLog.level(warn) << "Failed to set dexter_dsp_tx.stream0_start_clks = " << 0 << " : " << get_iio_error(r);
    }

    // Prepare streams
    constexpr int CHANNEL_INDEX = 0;
    m_tx_channel = iio_device_get_channel(m_ad9957_tx0, CHANNEL_INDEX);
    if (m_tx_channel == nullptr) {
        throw std::runtime_error("Dexter: Cannot create IIO channel.");
    }

    iio_channel_enable(m_tx_channel);

    m_buffer = iio_device_create_buffer(m_ad9957_tx0, IIO_BUFFER_LEN/sizeof(int16_t), 0);
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
    rs.num_overruns = 0;
    rs.num_late_packets = num_late;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}


double Dexter::get_real_secs(void) const
{
    struct timespec time_now;
    fill_time(&time_now);
    return (double)time_now.tv_sec + time_now.tv_nsec / 1000000000.0;

    /* We don't use actual device time, because we only have clock counter on pps edge available, not
     * current clock counter. */
#if 0
    long long pps_clks = 0;
    int r = 0;
    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "pps_clks", &pps_clks)) != 0) {
        etiLog.level(error) << "Failed to get dexter_dsp_tx.pps_clks: " << get_iio_error(r);
        throw std::runtime_error("Dexter: Cannot read IIO attribute");
    }

    return (double)m_utc_seconds_at_startup + (double)(pps_clks - m_clock_count_at_startup) / (double)DSP_CLOCK;
#endif
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
        frame_timestamp& ts,
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
    if (frame.buf.size() != TRANSMISSION_FRAME_LEN) {
        etiLog.level(debug) << "Dexter::transmit_frame Expected " <<
            TRANSMISSION_FRAME_LEN << " got " << frame.buf.size();
        throw std::runtime_error("Dexter: invalid buffer size");
    }

    const bool has_time_spec = (m_conf.enableSync and frame.ts.timestamp_valid);

    if (has_time_spec) {
        /*
        uint64_t timeS = frame.ts.timestamp_sec;
        etiLog.level(debug) << "Dexter: TS S " << timeS << " - " << m_utc_seconds_at_startup << " = " <<
            timeS - m_utc_seconds_at_startup;
        */

        // 10 because timestamp_pps is represented in 16.384 MHz clocks
        constexpr uint64_t TIMESTAMP_PPS_PER_DSP_CLOCKS = DSP_CLOCK / 16384000;
        uint64_t frame_ts_clocks =
            // at second level
            ((int64_t)frame.ts.timestamp_sec - (int64_t)m_utc_seconds_at_startup) * DSP_CLOCK + m_clock_count_at_startup +
            // at subsecond level
            (uint64_t)frame.ts.timestamp_pps * TIMESTAMP_PPS_PER_DSP_CLOCKS;

        long long pps_clks = 0;
        int r;
        if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "pps_clks", &pps_clks)) != 0) {
            etiLog.level(error) << "Failed to get dexter_dsp_tx.pps_clks: " << get_iio_error(r);
        }

        /*
        etiLog.level(debug) << "Dexter: TS CLK " <<
            ((int64_t)frame.ts.timestamp_sec - (int64_t)m_utc_seconds_at_startup) * DSP_CLOCK << " + " <<
            m_clock_count_at_startup << " + " <<
            (uint64_t)frame.ts.timestamp_pps * TIMESTAMP_PPS_PER_DSP_CLOCKS << " = " <<
            frame_ts_clocks << " DELTA " <<
            frame_ts_clocks << " - " << pps_clks << " = " <<
            (double)((int64_t)frame_ts_clocks - pps_clks) / DSP_CLOCK;
        */

        // Ensure we hand the frame over to HW at least 0.1s before timestamp
        if (((int64_t)frame_ts_clocks - pps_clks) < (int64_t)DSP_CLOCK / 10) {
            etiLog.level(warn) << "Skip frame short margin";
            num_late++;
            return;
        }


        if ((r = iio_device_attr_write_longlong(m_dexter_dsp_tx, "stream0_start_clks", frame_ts_clocks)) != 0) {
            etiLog.level(warn) << "Skip frame, failed to set dexter_dsp_tx.stream0_start_clks = " << frame_ts_clocks << " : " << get_iio_error(r);
            num_late++;
            return;
        }
    }

    // DabMod::launch_modulator ensures we get int16_t IQ here
    //const size_t num_samples = frame.buf.size() / (2*sizeof(int16_t));
    //const int16_t *buf = reinterpret_cast<const int16_t*>(frame.buf.data());

    for (size_t i = 0; i < IIO_BUFFERS; i++) {
        constexpr size_t buflen = TRANSMISSION_FRAME_LEN / IIO_BUFFERS;

        memcpy(iio_buffer_start(m_buffer), frame.buf.data() + (i * buflen), buflen);
        ssize_t pushed = iio_buffer_push(m_buffer);
        if (pushed < 0) {
            etiLog.level(error) << "Dexter: failed to push buffer " << get_iio_error(pushed);
        }
    }

    num_frames_modulated++;

    long long attr_value = 0;
    int r = 0;

    if ((r = iio_device_attr_read_longlong(m_dexter_dsp_tx, "buffer_underflows0", &attr_value)) == 0) {
        if ((size_t)attr_value != underflows and underflows != 0) {
            etiLog.level(warn) << "Dexter: underflow! " << underflows << " -> " << attr_value;
            underflows = attr_value;
        }
    }
}

} // namespace Output

#endif // HAVE_DEXTER


