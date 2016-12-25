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
#include <string>
#include <vector>
#include <list>
#include "eti.hpp"

namespace EdiDecoder {

// Information for Frame Characterisation available in
// EDI.
//
// Number of streams is given separately, and frame length
// is calculeted in the ETIWriter
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
    uint8_t scid;
    uint8_t sad;
    uint8_t tpl;
    std::vector<uint8_t> mst;

    // Return the length of the MST in multiples of 64 bits
    uint16_t stl(void) const { return mst.size() / 8; }
};

class ETIWriter {
    public:
        // Tell the ETIWriter what EDI protocol we receive in *ptr.
        // This is not part of the ETI data, but is used as check
        void update_protocol(
                const std::string& proto,
                uint16_t major,
                uint16_t minor);

        // Update the data for the frame characterisation
        void update_fc_data(const eti_fc_data& fc_data);

        void update_fic(const std::vector<uint8_t>& fic);

        void update_err(uint8_t err);

        // In addition to TSTA in ETI, EDI also transports more time
        // stamp information.
        void update_edi_time(
                uint32_t utco,
                uint32_t seconds);

        void update_mnsc(uint16_t mnsc);

        void add_subchannel(const eti_stc_data& stc);

        // Tell the ETIWriter that the AFPacket is complete
        void assemble(void);

        // Return the assembled ETI frame or an empty frame if not ready
        std::vector<uint8_t> getEtiFrame(void);

    private:
        void reinit(void);

        bool m_proto_valid = false;

        uint8_t m_err;

        bool m_fc_valid = false;
        eti_fc_data m_fc;

        // m_fic is valid if non-empty
        std::vector<uint8_t> m_fic;

        std::vector<uint8_t> m_etiFrame;

        std::list<eti_stc_data> m_subchannels;

        bool m_time_valid = false;
        uint32_t m_utco;
        uint32_t m_seconds;

        uint16_t m_mnsc = 0xffff;
};

}
