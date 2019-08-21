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
#include "ETIDecoder.hpp"
#include "buffer_unpack.hpp"
#include "crc.h"
#include "Log.h"
#include <cstdio>
#include <cassert>
#include <sstream>

namespace EdiDecoder {

using namespace std;

ETIDecoder::ETIDecoder(ETIDataCollector& data_collector, bool verbose) :
    m_data_collector(data_collector),
    m_dispatcher(std::bind(&ETIDecoder::packet_completed, this), verbose)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    m_dispatcher.register_tag("*ptr",
            std::bind(&ETIDecoder::decode_starptr, this, _1, _2));
    m_dispatcher.register_tag("deti",
            std::bind(&ETIDecoder::decode_deti, this, _1, _2));
    m_dispatcher.register_tag("est",
            std::bind(&ETIDecoder::decode_estn, this, _1, _2));
    m_dispatcher.register_tag("*dmy",
            std::bind(&ETIDecoder::decode_stardmy, this, _1, _2));
}

void ETIDecoder::push_bytes(const vector<uint8_t> &buf)
{
    m_dispatcher.push_bytes(buf);
}

void ETIDecoder::push_packet(const vector<uint8_t> &buf)
{
    m_dispatcher.push_packet(buf);
}

void ETIDecoder::setMaxDelay(int num_af_packets)
{
    m_dispatcher.setMaxDelay(num_af_packets);
}

#define AFPACKET_HEADER_LEN 10 // includes SYNC

bool ETIDecoder::decode_starptr(const vector<uint8_t> &value, uint16_t)
{
    if (value.size() != 0x40 / 8) {
        etiLog.log(warn, "Incorrect length %02lx for *PTR", value.size());
        return false;
    }

    char protocol_sz[5];
    protocol_sz[4] = '\0';
    copy(value.begin(), value.begin() + 4, protocol_sz);
    string protocol(protocol_sz);

    uint16_t major = read_16b(value.begin() + 4);
    uint16_t minor = read_16b(value.begin() + 6);

    m_data_collector.update_protocol(protocol, major, minor);

    return true;
}

bool ETIDecoder::decode_deti(const vector<uint8_t> &value, uint16_t)
{
    /*
    uint16_t detiHeader = fct | (fcth << 8) | (rfudf << 13) | (ficf << 14) | (atstf << 15);
    packet.push_back(detiHeader >> 8);
    packet.push_back(detiHeader & 0xFF);
    */

    uint16_t detiHeader = read_16b(value.begin());

    eti_fc_data fc;

    fc.atstf = (detiHeader >> 15) & 0x1;
    fc.ficf = (detiHeader >> 14) & 0x1;
    bool rfudf = (detiHeader >> 13) & 0x1;
    uint8_t fcth = (detiHeader >> 8) & 0x1F;
    uint8_t fct = detiHeader & 0xFF;

    fc.dflc = fcth * 250 + fct; // modulo 5000 counter

    uint32_t etiHeader = read_32b(value.begin() + 2);

    uint8_t stat = (etiHeader >> 24) & 0xFF;

    fc.mid = (etiHeader >> 22) & 0x03;
    fc.fp = (etiHeader >> 19) & 0x07;
    uint8_t rfa = (etiHeader >> 17) & 0x3;
    if (rfa != 0) {
        etiLog.log(warn, "EDI deti TAG: rfa non-zero");
    }

    bool rfu = (etiHeader >> 16) & 0x1;
    uint16_t mnsc = rfu ? 0xFFFF : etiHeader & 0xFFFF;

    const size_t fic_length_words = (fc.ficf ? (fc.mid == 3 ? 32 : 24) : 0);
    const size_t fic_length = 4 * fic_length_words;

    const size_t expected_length = 2 + 4 +
        (fc.atstf ? 1 + 4 + 3 : 0) +
        fic_length +
        (rfudf ? 3 : 0);

    if (value.size() != expected_length) {
        throw std::logic_error("EDI deti: Assertion error:"
                "value.size() != expected_length: " +
               to_string(value.size()) + " " +
               to_string(expected_length));
    }

    m_data_collector.update_err(stat);
    m_data_collector.update_mnsc(mnsc);

    size_t i = 2 + 4;

    if (fc.atstf) {
        uint8_t utco = value[i];
        i++;

        uint32_t seconds = read_32b(value.begin() + i);
        i += 4;

        m_data_collector.update_edi_time(utco, seconds);

        fc.tsta = read_24b(value.begin() + i);
        i += 3;
    }
    else {
        // Null timestamp, ETSI ETS 300 799, C.2.2
        fc.tsta = 0xFFFFFF;
    }


    if (fc.ficf) {
        vector<uint8_t> fic(fic_length);
        copy(   value.begin() + i,
                value.begin() + i + fic_length,
                fic.begin());
        i += fic_length;

        m_data_collector.update_fic(move(fic));
    }

    if (rfudf) {
        uint32_t rfud = read_24b(value.begin() + i);

        // high 16 bits: RFU in LIDATA EOH
        // low 8 bits: RFU in TIST (not supported)
        m_data_collector.update_rfu(rfud >> 8);
        if ((rfud & 0xFF) != 0xFF) {
            etiLog.level(warn) << "EDI: RFU in TIST not supported";
        }

        i += 3;
    }

    m_data_collector.update_fc_data(fc);

    return true;
}

bool ETIDecoder::decode_estn(const vector<uint8_t> &value, uint16_t n)
{
    uint32_t sstc = read_24b(value.begin());

    eti_stc_data stc;

    stc.stream_index = n - 1; // n is 1-indexed
    stc.scid = (sstc >> 18) & 0x3F;
    stc.sad = (sstc >> 8) & 0x3FF;
    stc.tpl = (sstc >> 2) & 0x3F;
    uint8_t rfa = sstc & 0x3;
    if (rfa != 0) {
        etiLog.level(warn) << "EDI: rfa field in ESTn tag non-null";
    }

    copy(   value.begin() + 3,
            value.end(),
            back_inserter(stc.mst));

    m_data_collector.add_subchannel(move(stc));

    return true;
}

bool ETIDecoder::decode_stardmy(const vector<uint8_t>& /*value*/, uint16_t)
{
    return true;
}

void ETIDecoder::packet_completed()
{
    m_data_collector.assemble();
}

}
