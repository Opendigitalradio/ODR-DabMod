/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   This presents a TCP socket to an external tool which calculates
   a Digital Predistortion model from a short sequence of transmit
   samples and corresponding receive samples.
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

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <vector>
#include <complex>
#include <cstring>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include "output/Feedback.h"
#include "Utils.h"
#include "Socket.h"

using namespace std;

namespace Output {

DPDFeedbackServer::DPDFeedbackServer(
        std::shared_ptr<SDRDevice> device,
        uint16_t port,
        uint32_t sampleRate) :
    m_port(port),
    m_sampleRate(sampleRate),
    m_device(device)
{
    if (m_port) {
        m_running.store(true);

        rx_burst_thread = std::thread(
                &DPDFeedbackServer::ReceiveBurstThread, this);

        burst_tcp_thread = std::thread(
                &DPDFeedbackServer::ServeFeedbackThread, this);
    }
}

DPDFeedbackServer::~DPDFeedbackServer()
{
    m_running.store(false);

    burstRequest.mutex_notification.notify_all();

    if (rx_burst_thread.joinable()) {
        rx_burst_thread.join();
    }

    if (burst_tcp_thread.joinable()) {
        burst_tcp_thread.join();
    }
}

void DPDFeedbackServer::set_tx_frame(
        const std::vector<uint8_t> &buf,
        const struct frame_timestamp &buf_ts)
{
    if (not m_running) {
        throw runtime_error("DPDFeedbackServer not running");
    }

    unique_lock<mutex> lock(burstRequest.mutex);

    if (buf.size() % sizeof(complexf) != 0) {
        throw logic_error("Buffer for tx frame has incorrect size");
    }

    if (burstRequest.state == BurstRequestState::SaveTransmitFrame) {
        const size_t n = std::min(
                burstRequest.num_samples * sizeof(complexf), buf.size());

        burstRequest.num_samples = n / sizeof(complexf);

        burstRequest.tx_samples.clear();
        burstRequest.tx_samples.resize(n);
        // A frame will always begin with the NULL symbol, which contains
        // no power. Instead of taking n samples at the beginning of the
        // frame, we take them at the end and adapt the timestamp accordingly.

        const size_t start_ix = buf.size() - n;
        copy(buf.begin() + start_ix, buf.end(), burstRequest.tx_samples.begin());

        frame_timestamp ts = buf_ts;
        ts += (1.0 * start_ix) / (sizeof(complexf) * m_sampleRate);

        burstRequest.tx_second = ts.timestamp_sec;
        burstRequest.tx_pps = ts.timestamp_pps;

        // Prepare the next state
        burstRequest.rx_second = ts.timestamp_sec;
        burstRequest.rx_pps = ts.timestamp_pps;
        burstRequest.state = BurstRequestState::SaveReceiveFrame;

        lock.unlock();
        burstRequest.mutex_notification.notify_one();
    }
    else {
        lock.unlock();
    }
}

void DPDFeedbackServer::ReceiveBurstThread()
{
    try {
        set_thread_name("dpdreceiveburst");

        while (m_running) {
            unique_lock<mutex> lock(burstRequest.mutex);
            while (burstRequest.state != BurstRequestState::SaveReceiveFrame) {
                if (not m_running) break;
                burstRequest.mutex_notification.wait(lock);
            }

            if (not m_running) break;

            const size_t num_samps = burstRequest.num_samples;

            frame_timestamp ts;
            ts.timestamp_sec = burstRequest.rx_second;
            ts.timestamp_pps = burstRequest.rx_pps;
            ts.timestamp_valid = true;

            // We need to free the mutex while we recv(), because otherwise we block the
            // TX thread
            lock.unlock();

            const double device_time = m_device->get_real_secs();
            const double cmd_time = ts.get_real_secs();

            std::vector<uint8_t> buf(num_samps * sizeof(complexf));

            const double timeout = 60;
            size_t samples_read = m_device->receive_frame(
                    reinterpret_cast<complexf*>(buf.data()),
                    num_samps, ts, timeout);

            lock.lock();
            burstRequest.rx_samples = std::move(buf);
            burstRequest.rx_samples.resize(samples_read * sizeof(complexf));

            // The recv might have happened at another time than requested
            burstRequest.rx_second = ts.timestamp_sec;
            burstRequest.rx_pps = ts.timestamp_pps;

            etiLog.level(debug) << "DPD: acquired " << samples_read <<
                " RX feedback samples " <<
                "at time " << burstRequest.tx_second << " + " <<
                std::fixed << burstRequest.tx_pps / 16384000.0 <<
                " Delta=" << cmd_time - device_time;

            burstRequest.state = BurstRequestState::Acquired;

            lock.unlock();
            burstRequest.mutex_notification.notify_one();
        }
    }
    catch (const runtime_error &e) {
        etiLog.level(error) << "DPD Feedback RX runtime error: " << e.what();
    }
    catch (const std::exception &e) {
        etiLog.level(error) << "DPD Feedback RX exception: " << e.what();
    }
    catch (...) {
        etiLog.level(error) << "DPD Feedback RX unknown exception!";
    }

    m_running.store(false);
}

void DPDFeedbackServer::ServeFeedback()
{
    Socket::TCPSocket m_server_sock;
    m_server_sock.listen(m_port, "127.0.0.1");

    etiLog.level(info) << "DPD Feedback server listening on port " << m_port;

    while (m_running) {
        auto client_sock = m_server_sock.accept(1000);

        if (not m_running) {
            break;
        }

        if (not client_sock.valid()) {
            // No connection request received
            continue;
        }

        uint8_t request_version = 0;
        ssize_t read = client_sock.recv(&request_version, 1, 0);
        if (!read) break; // done reading
        if (read < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client read request version failed: " << strerror(errno);
            break;
        }

        if (request_version != 1) {
            etiLog.level(info) << "DPD Feedback Server wrong request version";
            break;
        }

        uint32_t num_samples = 0;
        read = client_sock.recv(&num_samples, 4, 0);
        if (!read) break; // done reading
        if (read < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client read num samples failed";
            break;
        }

        // We are ready to issue the request now
        {
            unique_lock<mutex> lock(burstRequest.mutex);
            burstRequest.num_samples = num_samples;
            burstRequest.state = BurstRequestState::SaveTransmitFrame;
        }

        // Wait for the result to be ready
        unique_lock<mutex> lock(burstRequest.mutex);
        while (burstRequest.state != BurstRequestState::Acquired) {
            if (not m_running) break;
            burstRequest.mutex_notification.wait(lock);
        }

        burstRequest.state = BurstRequestState::None;
        lock.unlock();

        burstRequest.num_samples = std::min(burstRequest.num_samples,
                std::min(
                    burstRequest.tx_samples.size() / sizeof(complexf),
                    burstRequest.rx_samples.size() / sizeof(complexf)));

        uint32_t num_samples_32 = burstRequest.num_samples;
        if (client_sock.sendall(&num_samples_32, sizeof(num_samples_32)) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send num_samples failed";
            break;
        }

        if (client_sock.sendall(
                    &burstRequest.tx_second,
                    sizeof(burstRequest.tx_second)) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send tx_second failed";
            break;
        }

        if (client_sock.sendall(
                    &burstRequest.tx_pps,
                    sizeof(burstRequest.tx_pps)) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send tx_pps failed";
            break;
        }

        const size_t frame_bytes = burstRequest.num_samples * sizeof(complexf);

        if (burstRequest.tx_samples.size() < frame_bytes) {
            throw logic_error("DPD Feedback burstRequest invalid: not enough TX samples");
        }

        if (client_sock.sendall(
                    &burstRequest.tx_samples[0],
                    frame_bytes) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send tx_frame failed";
            break;
        }

        if (client_sock.sendall(
                    &burstRequest.rx_second,
                    sizeof(burstRequest.rx_second)) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send rx_second failed";
            break;
        }

        if (client_sock.sendall(
                    &burstRequest.rx_pps,
                    sizeof(burstRequest.rx_pps)) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send rx_pps failed";
            break;
        }

        if (burstRequest.rx_samples.size() < frame_bytes) {
            throw logic_error("DPD Feedback burstRequest invalid: not enough RX samples");
        }

        if (client_sock.sendall(
                    &burstRequest.rx_samples[0],
                    frame_bytes) < 0) {
            etiLog.level(info) <<
                "DPD Feedback Server Client send rx_frame failed";
            break;
        }
    }
}

void DPDFeedbackServer::ServeFeedbackThread()
{
    set_thread_name("dpdfeedbackserver");

    while (m_running) {
        try {
            ServeFeedback();
        }
        catch (const runtime_error &e) {
            etiLog.level(error) << "DPD Feedback Server runtime error: " << e.what();
        }
        catch (const std::exception &e) {
            etiLog.level(error) << "DPD Feedback Server exception: " << e.what();
        }
        catch (...) {
            etiLog.level(error) << "DPD Feedback Server unknown exception!";
        }

        if (m_running) {
            this_thread::sleep_for(chrono::seconds(5));
        }
    }

    m_running.store(false);
}

} // namespace Output
