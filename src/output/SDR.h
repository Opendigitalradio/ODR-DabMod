/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   Common interface for all SDR outputs
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

#include <chrono>
#include "ModPlugin.h"
#include "EtiReader.h"
#include "output/SDRDevice.h"
#include "output/Feedback.h"

namespace Output {

using complexf = std::complex<float>;

class SDR : public ModOutput, public ModMetadata, public RemoteControllable {
    public:
        SDR(SDRDeviceConfig& config, std::shared_ptr<SDRDevice> device);
        SDR(const SDR& other) = delete;
        SDR operator=(const SDR& other) = delete;
        virtual ~SDR();

        virtual int process(Buffer *dataIn) override;
        virtual meta_vec_t process_metadata(const meta_vec_t& metadataIn) override;

        virtual const char* name() override;

        /*********** REMOTE CONTROL ***************/

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value) override;

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const override;

    private:
        void process_thread_entry(void);
        void handle_frame(struct FrameData &frame);
        void sleep_through_frame(void);

        SDRDeviceConfig& m_config;

        std::atomic<bool> m_running = ATOMIC_VAR_INIT(false);
        std::thread m_device_thread;
        std::vector<uint8_t> m_frame;
        ThreadsafeQueue<FrameData> m_queue;

        std::shared_ptr<SDRDevice> m_device;
        std::string m_name;

        std::shared_ptr<DPDFeedbackServer> m_dpd_feedback_server;

        bool     last_tx_time_initialised = false;
        uint32_t last_tx_second = 0;
        uint32_t last_tx_pps = 0;

        bool     t_last_frame_initialised = false;
        std::chrono::steady_clock::time_point t_last_frame;
};

}

