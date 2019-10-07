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

#include "eti.hpp"
#include "common.hpp"
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace EdiDecoder {

// Information for Frame Characterisation available in
// EDI.
//
// Number of streams is given separately, and frame length
// is calculated in the ETIDataCollector
struct eti_fc_data {
    bool atstf;
    uint32_t tsta;
    bool ficf;
    uint16_t dflc;
    uint8_t mid;
    uint8_t fp;

    uint8_t fct(void) const { return dflc % 250; }
};

// Information for a subchannel available in EDI
struct eti_stc_data {
    uint8_t stream_index;
    uint8_t scid;
    uint16_t sad;
    uint8_t tpl;
    std::vector<uint8_t> mst;

    // Return the length of the MST in multiples of 64 bits
    uint16_t stl(void) const { return mst.size() / 8; }
};

/* A class that receives multiplex data must implement the interface described
 * in the ETIDataCollector. This can be e.g. a converter to ETI, or something that
 * prepares data structures for a modulator.
 */
class ETIDataCollector {
    public:
        // Tell the ETIWriter what EDI protocol we receive in *ptr.
        // This is not part of the ETI data, but is used as check
        virtual void update_protocol(
                const std::string& proto,
                uint16_t major,
                uint16_t minor) = 0;

        // Update the data for the frame characterisation
        virtual void update_fc_data(const eti_fc_data& fc_data) = 0;

        virtual void update_fic(std::vector<uint8_t>&& fic) = 0;

        virtual void update_err(uint8_t err) = 0;

        // In addition to TSTA in ETI, EDI also transports more time
        // stamp information.
        virtual void update_edi_time(uint32_t utco, uint32_t seconds) = 0;

        virtual void update_mnsc(uint16_t mnsc) = 0;

        virtual void update_rfu(uint16_t rfu) = 0;

        virtual void add_subchannel(eti_stc_data&& stc) = 0;

        // Tell the ETIWriter that the AFPacket is complete
        virtual void assemble(void) = 0;
};

/* The ETIDecoder takes care of decoding the EDI TAGs related to the transport
 * of ETI(NI) data inside AF and PF packets.
 *
 * PF packets are handed over to the PFT decoder, which will in turn return
 * AF packets. AF packets are directly handled (TAG extraction) here.
 */
class ETIDecoder {
    public:
        ETIDecoder(ETIDataCollector& data_collector, bool verbose);

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
        bool decode_starptr(const std::vector<uint8_t> &value, uint16_t);
        bool decode_deti(const std::vector<uint8_t> &value, uint16_t);
        bool decode_estn(const std::vector<uint8_t> &value, uint16_t n);
        bool decode_stardmy(const std::vector<uint8_t> &value, uint16_t);

        void packet_completed();

        ETIDataCollector& m_data_collector;
        TagDispatcher m_dispatcher;

};

}
