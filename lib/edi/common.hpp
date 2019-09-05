/*
   Copyright (C) 2019
   Matthias P. Braendli, matthias.braendli@mpb.li

   http://opendigitalradio.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#pragma once

#include "PFT.hpp"
#include <functional>
#include <map>
#include <chrono>
#include <string>
#include <vector>
#include <cstddef>
#include <ctime>

namespace EdiDecoder {

struct frame_timestamp_t {
    uint32_t seconds = 0;
    uint32_t utco = 0;
    uint32_t tsta = 0; // According to EN 300 797 Annex B

    bool valid() const;
    std::string to_string() const;
    std::time_t to_unix_epoch() const;
    std::chrono::system_clock::time_point to_system_clock() const;

    double diff_ms(const frame_timestamp_t& other) const;

    static frame_timestamp_t from_unix_epoch(std::time_t time, uint32_t tai_utc_offset, uint32_t tsta);
};


struct decode_state_t {
    decode_state_t(bool _complete, size_t _num_bytes_consumed) :
        complete(_complete), num_bytes_consumed(_num_bytes_consumed) {}
    bool complete;
    size_t num_bytes_consumed;
};

/* The TagDispatcher takes care of decoding EDI, with or without PFT, and
 * will call functions when TAGs are encountered.
 *
 * PF packets are handed over to the PFT decoder, which will in turn return
 * AF packets. AF packets are directly dispatched to the TAG functions.
 */
class TagDispatcher {
    public:
        TagDispatcher(std::function<void()>&& af_packet_completed, bool verbose);

        /* Push bytes into the decoder. The buf can contain more
         * than a single packet. This is useful when reading from streams
         * (files, TCP)
         */
        void push_bytes(const std::vector<uint8_t> &buf);

        /* Push a complete packet into the decoder. Useful for UDP and other
         * datagram-oriented protocols.
         */
        void push_packet(const std::vector<uint8_t> &buf);

        /* Set the maximum delay in number of AF Packets before we
         * abandon decoding a given pseq.
         */
        void setMaxDelay(int num_af_packets);

        using tag_handler = std::function<bool(std::vector<uint8_t>, uint16_t)>;
        void register_tag(const std::string& tag, tag_handler&& h);

    private:
        decode_state_t decode_afpacket(const std::vector<uint8_t> &input_data);
        bool decode_tagpacket(const std::vector<uint8_t> &payload);

        PFT::PFT m_pft;
        uint16_t m_last_seq = 0;
        std::vector<uint8_t> m_input_data;
        std::map<std::string, tag_handler> m_handlers;
        std::function<void()> m_af_packet_completed;
};

}
