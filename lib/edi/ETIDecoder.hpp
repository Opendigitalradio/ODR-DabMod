/*
   Copyright (C) 2016
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

#include <stdint.h>
#include <deque>
#include <string>
#include <vector>
#include "ETIWriter.hpp"
#include "PFT.hpp"

namespace EdiDecoder {

struct decode_state_t {
    decode_state_t(bool _complete, size_t _num_bytes_consumed) :
        complete(_complete), num_bytes_consumed(_num_bytes_consumed) {}
    bool complete;
    size_t num_bytes_consumed;
};

class ETIDecoder {
    public:
        ETIDecoder(ETIWriter& eti_writer);

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

    private:
        decode_state_t decode_afpacket(const std::vector<uint8_t> &input_data);
        bool decode_tagpacket(const std::vector<uint8_t> &payload);
        bool decode_starptr(const std::vector<uint8_t> &value);
        bool decode_deti(const std::vector<uint8_t> &value);
        bool decode_estn(const std::vector<uint8_t> &value, uint8_t n);
        bool decode_stardmy(const std::vector<uint8_t> &value);

        ETIWriter& m_eti_writer;

        PFT::PFT m_pft;

        std::vector<uint8_t> m_input_data;
};

}
