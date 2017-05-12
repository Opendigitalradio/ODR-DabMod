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

#ifdef HAVE_OUTPUT_UHD

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <string>

#include "Log.h"
#include "TimestampDecoder.h"

enum class BurstRequestState {
    None, // To pending request
    SaveTransmitFrame, // The TX thread has to save an outgoing frame
    SaveReceiveFrame, // The RX thread has to save an incoming frame
    Acquired, // Both TX and RX frames are ready
};

struct UHDReceiveBurstRequest {
    // All fields in this struct are protected
    mutable boost::mutex mutex;
    boost::condition_variable mutex_notification;

    BurstRequestState state;

    // In the SaveTransmit states, frame_length samples are saved into
    // the vectors
    size_t frame_length;

    // The timestamp of the first sample of the TX buffers
    uint32_t tx_second;
    uint32_t tx_pps; // in units of 1/16384000s

    std::vector<uint8_t> tx_samples;

    // The timestamp of the first sample of the RX buffers
    uint32_t rx_second;
    uint32_t rx_pps;

    std::vector<uint8_t> rx_samples;
};

// Serve TX samples and RX feedback samples over a TCP connection
class OutputUHDFeedback {
    public:
        OutputUHDFeedback();
        OutputUHDFeedback(const OutputUHDFeedback& other) = delete;
        OutputUHDFeedback& operator=(const OutputUHDFeedback& other) = delete;
        ~OutputUHDFeedback();

        void setup(uhd::usrp::multi_usrp::sptr usrp, uint16_t port, uint32_t sampleRate);

        void set_tx_frame(const std::vector<uint8_t> &buf,
                const struct frame_timestamp& ts);

    private:
        // Thread that reacts to burstRequests and receives from the USRP
        void ReceiveBurstThread(void);

        // Thread that listens for requests over TCP to get TX and RX feedback
        void ServeFeedbackThread(void);

        boost::thread rx_burst_thread;
        boost::thread burst_tcp_thread;

        UHDReceiveBurstRequest burstRequest;

        bool m_running = false;
        uint16_t m_port = 0;
        uint32_t m_sampleRate = 0;
        uhd::usrp::multi_usrp::sptr m_usrp;
};


#endif // HAVE_OUTPUT_UHD
