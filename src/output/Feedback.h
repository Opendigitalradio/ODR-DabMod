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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <string>
#include <atomic>

#include "Log.h"
#include "TimestampDecoder.h"
#include "output/SDRDevice.h"

namespace Output {

enum class BurstRequestState {
    None, // To pending request
    SaveTransmitFrame, // The TX thread has to save an outgoing frame
    SaveReceiveFrame, // The RX thread has to save an incoming frame
    Acquired, // Both TX and RX frames are ready
};

struct FeedbackBurstRequest {
    // All fields in this struct are protected
    mutable std::mutex mutex;
    std::condition_variable mutex_notification;

    BurstRequestState state = BurstRequestState::None;

    // In the SaveTransmit states, num_samples complexf samples are saved into
    // the vectors
    size_t num_samples = 0;

    // The timestamp of the first sample of the TX buffers
    uint32_t tx_second = 0;
    uint32_t tx_pps = 0; // in units of 1/16384000s

    // Samples contain complexf, but since our internal representation is uint8_t
    // we keep it like that
    std::vector<uint8_t> tx_samples;

    // The timestamp of the first sample of the RX buffers
    uint32_t rx_second = 0;
    uint32_t rx_pps = 0;

    std::vector<uint8_t> rx_samples; // Also, actually complexf
};

// Serve TX samples and RX feedback samples over a TCP connection
class DPDFeedbackServer {
    public:
        DPDFeedbackServer(
                std::shared_ptr<SDRDevice> device,
                uint16_t port, // Set to 0 to disable the Feedbackserver
                uint32_t sampleRate);
        DPDFeedbackServer(const DPDFeedbackServer& other) = delete;
        DPDFeedbackServer& operator=(const DPDFeedbackServer& other) = delete;
        ~DPDFeedbackServer();

        void set_tx_frame(const std::vector<uint8_t> &buf,
                const struct frame_timestamp& ts);

    private:
        // Thread that reacts to burstRequests and receives from the SDR device
        void ReceiveBurstThread(void);

        // Thread that listens for requests over TCP to get TX and RX feedback
        void ServeFeedbackThread(void);
        void ServeFeedback(void);

        std::thread rx_burst_thread;
        std::thread burst_tcp_thread;

        FeedbackBurstRequest burstRequest;

        std::atomic_bool m_running;
        uint16_t m_port = 0;
        uint32_t m_sampleRate = 0;
        std::shared_ptr<SDRDevice> m_device;
};

} // namespace Output
