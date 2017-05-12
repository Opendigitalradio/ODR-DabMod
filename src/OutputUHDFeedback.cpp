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

#ifdef HAVE_OUTPUT_UHD

#include <vector>
#include <complex>
#include <cstring>
#include <uhd/types/stream_cmd.hpp>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include "OutputUHDFeedback.h"
#include "Utils.h"

using namespace std;
typedef std::complex<float> complexf;

OutputUHDFeedback::OutputUHDFeedback()
{
    m_running.store(false);
}

void OutputUHDFeedback::setup(uhd::usrp::multi_usrp::sptr usrp, uint16_t port, uint32_t sampleRate)
{
    m_usrp = usrp;
    m_sampleRate = sampleRate;
    burstRequest.state = BurstRequestState::None;

    if (port) {
        m_port = port;
        m_running.store(true);

        rx_burst_thread = boost::thread(&OutputUHDFeedback::ReceiveBurstThread, this);
        burst_tcp_thread = boost::thread(&OutputUHDFeedback::ServeFeedbackThread, this);
    }
}

OutputUHDFeedback::~OutputUHDFeedback()
{
    m_running.store(false);

    rx_burst_thread.interrupt();
    burst_tcp_thread.interrupt();

    rx_burst_thread.join();
    burst_tcp_thread.join();
}

void OutputUHDFeedback::set_tx_frame(
        const std::vector<uint8_t> &buf,
        const struct frame_timestamp &buf_ts)
{
    boost::mutex::scoped_lock lock(burstRequest.mutex);

    assert(buf.size() % sizeof(complexf) == 0);

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

void OutputUHDFeedback::ReceiveBurstThread()
{
    set_thread_name("uhdreceiveburst");

    uhd::stream_args_t stream_args("fc32"); //complex floats
    auto rxStream = m_usrp->get_rx_stream(stream_args);

    while (m_running) {
        boost::mutex::scoped_lock lock(burstRequest.mutex);
        while (burstRequest.state != BurstRequestState::SaveReceiveFrame) {
            if (not m_running) break;
            burstRequest.mutex_notification.wait(lock);
        }

        if (not m_running) break;

        uhd::stream_cmd_t cmd(
                uhd::stream_cmd_t::stream_mode_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
        cmd.num_samps = burstRequest.num_samples;
        cmd.stream_now = false;

        double pps = burstRequest.rx_pps / 16384000.0;
        cmd.time_spec = uhd::time_spec_t(burstRequest.rx_second, pps);

        rxStream->issue_stream_cmd(cmd);

        uhd::rx_metadata_t md;
        burstRequest.rx_samples.resize(burstRequest.num_samples * sizeof(complexf));
        rxStream->recv(&burstRequest.rx_samples[0], burstRequest.num_samples, md);

        // The recv might have happened at another time than requested
        burstRequest.rx_second = md.time_spec.get_full_secs();
        burstRequest.rx_pps = md.time_spec.get_frac_secs() * 16384000.0;

        burstRequest.state = BurstRequestState::Acquired;

        lock.unlock();
        burstRequest.mutex_notification.notify_one();
    }
}

static int accept_with_timeout(int server_socket, int timeout_ms, struct sockaddr_in *client)
{
    struct pollfd fds[1];
    fds[0].fd = server_socket;
    fds[0].events = POLLIN | POLLOUT;

    int retval = poll(fds, 1, timeout_ms);

    if (retval == -1) {
        throw std::runtime_error("TCP Socket accept error: " + to_string(errno));
    }
    else if (retval) {
        socklen_t client_len = sizeof(struct sockaddr_in);
        return accept(server_socket, (struct sockaddr*)&client, &client_len);
    }
    else {
        return -2;
    }
}

void OutputUHDFeedback::ServeFeedbackThread()
{
    set_thread_name("uhdservefeedback");

    try {
        if ((m_server_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            throw std::runtime_error("Can't create TCP socket");
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(m_server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Can't bind TCP socket");
        }

        if (listen(m_server_sock, 1) < 0) {
            throw std::runtime_error("Can't listen TCP socket");
        }

        while (m_running) {
            struct sockaddr_in client;
            int client_sock = accept_with_timeout(m_server_sock, 1000, &client);

            if (client_sock == -1) {
                throw runtime_error("Could not establish new connection");
            }
            else if (client_sock == -2) {
                continue;
            }

            uint8_t request_version = 0;
            ssize_t read = recv(client_sock, &request_version, 1, 0);
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
            read = recv(client_sock, &num_samples, 4, 0);
            if (!read) break; // done reading
            if (read < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client read num samples failed";
                break;
            }

            // We are ready to issue the request now
            {
                boost::mutex::scoped_lock lock(burstRequest.mutex);
                burstRequest.num_samples = num_samples;
                burstRequest.state = BurstRequestState::SaveTransmitFrame;

                lock.unlock();
            }

            // Wait for the result to be ready
            boost::mutex::scoped_lock lock(burstRequest.mutex);
            while (burstRequest.state != BurstRequestState::Acquired) {
                if (not m_running) break;
                burstRequest.mutex_notification.wait(lock);
            }

            burstRequest.state = BurstRequestState::None;
            lock.unlock();

            if (send(client_sock,
                        &burstRequest.num_samples,
                        sizeof(burstRequest.num_samples),
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send num_samples failed";
                break;
            }

            if (send(client_sock,
                        &burstRequest.tx_second,
                        sizeof(burstRequest.tx_second),
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send tx_second failed";
                break;
            }

            if (send(client_sock,
                        &burstRequest.tx_pps,
                        sizeof(burstRequest.tx_pps),
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send tx_pps failed";
                break;
            }

            const size_t frame_bytes = burstRequest.num_samples * sizeof(complexf);

            assert(burstRequest.tx_samples.size() == frame_bytes);
            if (send(client_sock,
                        &burstRequest.tx_samples[0],
                        frame_bytes,
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send tx_frame failed";
                break;
            }

            if (send(client_sock,
                        &burstRequest.rx_second,
                        sizeof(burstRequest.rx_second),
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send rx_second failed";
                break;
            }

            if (send(client_sock,
                        &burstRequest.rx_pps,
                        sizeof(burstRequest.rx_pps),
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send rx_pps failed";
                break;
            }

            assert(burstRequest.rx_samples.size() == frame_bytes);
            if (send(client_sock,
                        &burstRequest.rx_samples[0],
                        frame_bytes,
                        0) < 0) {
                etiLog.level(info) <<
                    "DPD Feedback Server Client send rx_frame failed";
                break;
            }

            close(client_sock);
        }
    }
    catch (runtime_error &e) {
        etiLog.level(error) << "DPD Feedback Server fault: " << e.what();
    }

    m_running = false;

    if (m_server_sock != -1) {
        close(m_server_sock);
        m_server_sock = -1;
    }
}

#endif
