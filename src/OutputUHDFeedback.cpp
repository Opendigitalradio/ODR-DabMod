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

#include <vector>
#include <uhd/types/stream_cmd.hpp>
#include <sys/socket.h>
#include "OutputUHDFeedback.h"
#include "Utils.h"

using namespace std;
typedef std::complex<float> complexf;

OutputUHDFeedback::OutputUHDFeedback()
{
    running = false;
}

void OutputUHDFeedback::setup(uhd::usrp::multi_usrp::sptr usrp, uint16_t port)
{
    myUsrp = usrp;
    burstRequest.state = BurstRequestState::None;

    if (port) {
        m_port = port;
        running = true;

        rx_burst_thread = boost::thread(&OutputUHDFeedback::ReceiveBurstThread, this);
        burst_tcp_thread = boost::thread(&OutputUHDFeedback::ServeFeedbackThread, this);
    }
}

OutputUHDFeedback::~OutputUHDFeedback()
{
    running = false;
    rx_burst_thread.join();
    burst_tcp_thread.join();
}

void OutputUHDFeedback::set_tx_frame(
        const std::vector<uint8_t> &buf,
        const struct frame_timestamp& ts)
{
    boost::mutex::scoped_lock lock(burstRequest.mutex);

    if (burstRequest.state == BurstRequestState::SaveTransmitFrame) {
        const size_t n = std::min(
                burstRequest.frame_length * sizeof(complexf), buf.size());

        burstRequest.tx_samples.clear();
        burstRequest.tx_samples.resize(n);
        copy(buf.begin(), buf.begin() + n, burstRequest.tx_samples.begin());

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
    auto rxStream = myUsrp->get_rx_stream(stream_args);

    while (running) {
        boost::mutex::scoped_lock lock(burstRequest.mutex);
        while (burstRequest.state != BurstRequestState::SaveReceiveFrame) {
            if (not running) break;
            burstRequest.mutex_notification.wait(lock);
        }

        if (not running) break;

        uhd::stream_cmd_t cmd(
                uhd::stream_cmd_t::stream_mode_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
        cmd.num_samps = burstRequest.frame_length;
        cmd.stream_now = false;

        double pps = burstRequest.rx_pps / 16384000.0;
        cmd.time_spec = uhd::time_spec_t(burstRequest.rx_second, pps);

        rxStream->issue_stream_cmd(cmd);

        uhd::rx_metadata_t md;
        burstRequest.rx_samples.resize(burstRequest.frame_length * sizeof(complexf));
        rxStream->recv(&burstRequest.rx_samples[0], burstRequest.frame_length, md);

        burstRequest.rx_second = md.time_spec.get_full_secs();
        burstRequest.rx_pps = md.time_spec.get_frac_secs() * 16384000.0;

        burstRequest.state = BurstRequestState::Acquired;

        lock.unlock();
        burstRequest.mutex_notification.notify_one();
    }
}

void OutputUHDFeedback::ServeFeedbackThread()
{
    set_thread_name("uhdservefeedback");

    int server_sock = -1;
    try {
        if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            throw std::runtime_error("Can't create TCP socket");
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("Can't bind TCP socket");
        }

        if (listen(server_sock, 1) < 0) {
            throw std::runtime_error("Can't listen TCP socket");
        }

        while (running) {
            struct sockaddr_in client;
            socklen_t client_len = sizeof(client);
            int client_sock = accept(server_sock,
                    (struct sockaddr*)&client, &client_len);

            if (client_sock < 0) {
                throw runtime_error("Could not establish new connection");
            }

            while (running) {
                uint8_t request_version = 0;
                int read = recv(client_sock, &request_version, 1, 0);
                if (!read) break; // done reading
                if (read < 0) {
                    etiLog.level(info) <<
                        "DPD Feedback Server Client read request verson failed";
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
                }

                // We are ready to issue the request now
                {
                    boost::mutex::scoped_lock lock(burstRequest.mutex);
                    burstRequest.frame_length = num_samples;
                    burstRequest.state = BurstRequestState::SaveTransmitFrame;

                    lock.unlock();
                }

                // Wait for the result to be ready
                boost::mutex::scoped_lock lock(burstRequest.mutex);
                while (burstRequest.state != BurstRequestState::Acquired) {
                    if (not running) break;
                    burstRequest.mutex_notification.wait(lock);
                }

                burstRequest.state = BurstRequestState::None;
                lock.unlock();

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

#warning "Send buf"
            }

            close(client_sock);
        }
    }
    catch (runtime_error &e) {
        etiLog.level(error) << "DPD Feedback Server fault: " << e.what();
    }

    running = false;

    if (server_sock != -1) {
        close(server_sock);
    }
}
