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
#include "ETIDecoder.hpp"
#include "buffer_unpack.hpp"
#include "crc.h"
#include "Log.h"
#include <stdio.h>
#include <cassert>
#include <sstream>

namespace EdiDecoder {

using namespace std;

ETIDecoder::ETIDecoder(ETIWriter& eti_writer) :
    m_eti_writer(eti_writer)
{
}

void ETIDecoder::push_bytes(const vector<uint8_t> &buf)
{
    copy(buf.begin(), buf.end(), back_inserter(m_input_data));

    while (m_input_data.size() > 2) {
        if (m_input_data[0] == 'A' and m_input_data[1] == 'F') {
            decode_state_t st = decode_afpacket(m_input_data);

            if (st.num_bytes_consumed == 0 and not st.complete) {
                // We need to refill our buffer
                break;
            }

            if (st.num_bytes_consumed) {
                vector<uint8_t> remaining_data;
                copy(m_input_data.begin() + st.num_bytes_consumed,
                        m_input_data.end(),
                        back_inserter(remaining_data));
                m_input_data = remaining_data;
            }

            if (st.complete) {
                m_eti_writer.assemble();
            }

        }
        else if (m_input_data[0] == 'P' and m_input_data[1] == 'F') {
            PFT::Fragment fragment;
            const size_t fragment_bytes = fragment.loadData(m_input_data);

            if (fragment_bytes == 0) {
                // We need to refill our buffer
                break;
            }

            vector<uint8_t> remaining_data;
            copy(m_input_data.begin() + fragment_bytes,
                    m_input_data.end(),
                    back_inserter(remaining_data));
            m_input_data = remaining_data;

            if (fragment.isValid()) {
                m_pft.pushPFTFrag(fragment);
            }

            auto af = m_pft.getNextAFPacket();
            if (not af.empty()) {
                decode_state_t st = decode_afpacket(af);

                if (st.complete) {
                    m_eti_writer.assemble();
                }
            }

        }
        else {
            etiLog.log(warn,"Unknown %c!", *m_input_data.data());
            m_input_data.erase(m_input_data.begin());
        }
    }
}

void ETIDecoder::push_packet(const vector<uint8_t> &buf)
{
    if (buf.size() < 2) {
        throw std::invalid_argument("Not enough bytes to read EDI packet header");
    }

    if (buf[0] == 'A' and buf[1] == 'F') {
        const decode_state_t st = decode_afpacket(buf);

        if (st.complete) {
            m_eti_writer.assemble();
        }

    }
    else if (buf[0] == 'P' and buf[1] == 'F') {
        PFT::Fragment fragment;
        fragment.loadData(buf);

        if (fragment.isValid()) {
            m_pft.pushPFTFrag(fragment);
        }

        auto af = m_pft.getNextAFPacket();
        if (not af.empty()) {
            decode_state_t st = decode_afpacket(af);

            if (st.complete) {
                m_eti_writer.assemble();
            }
        }
    }
    else {
        const char packettype[3] = {(char)buf[0], (char)buf[1], '\0'};
        std::stringstream ss;
        ss << "Unknown EDI packet ";
        ss << packettype;
        throw std::invalid_argument(ss.str());
    }
}

void ETIDecoder::setMaxDelay(int num_af_packets)
{
    m_pft.setMaxDelay(num_af_packets);
}

#define AFPACKET_HEADER_LEN 10 // includes SYNC

decode_state_t ETIDecoder::decode_afpacket(
        const std::vector<uint8_t> &input_data)
{
    if (input_data.size() < AFPACKET_HEADER_LEN) {
        return {false, 0};
    }

    // read length from packet
    uint32_t taglength = read_32b(input_data.begin() + 2);
    uint16_t seq = read_16b(input_data.begin() + 6);
    if (m_last_seq + 1 != seq) {
        etiLog.level(warn) << "EDI AF Packet sequence error";
    }
    m_last_seq = seq;

    bool has_crc = (input_data[8] & 0x80) ? true : false;
    uint8_t major_revision = (input_data[8] & 0x70) >> 4;
    uint8_t minor_revision = input_data[8] & 0x0F;
    if (major_revision != 1 or minor_revision != 0) {
        throw invalid_argument("EDI AF Packet has wrong revision " +
                to_string(major_revision) + "." + to_string(minor_revision));
    }
    uint8_t pt = input_data[9];
    if (pt != 'T') {
        // only support Tag
        return {false, 0};
    }

    const size_t crclength = 2;
    if (input_data.size() < AFPACKET_HEADER_LEN + taglength + crclength) {
        return {false, 0};
    }

    if (not has_crc) {
        throw invalid_argument("AF packet not supported, has no CRC");
    }

    uint16_t crc = 0xffff;
    for (size_t i = 0; i < AFPACKET_HEADER_LEN + taglength; i++) {
        crc = crc16(crc, &input_data[i], 1);
    }
    crc ^= 0xffff;

    uint16_t packet_crc = read_16b(input_data.begin() + AFPACKET_HEADER_LEN + taglength);

    if (packet_crc != crc) {
        throw invalid_argument(
                "AF Packet crc wrong");
    }
    else {
        vector<uint8_t> payload(taglength);
        copy(input_data.begin() + AFPACKET_HEADER_LEN,
                input_data.begin() + AFPACKET_HEADER_LEN + taglength,
                payload.begin());

        return {decode_tagpacket(payload),
            AFPACKET_HEADER_LEN + taglength + 2};
    }
}

bool ETIDecoder::decode_tagpacket(const vector<uint8_t> &payload)
{
    size_t length = 0;

    bool success = true;

    for (size_t i = 0; i + 8 < payload.size(); i += 8 + length) {
        char tag_sz[5];
        tag_sz[4] = '\0';
        copy(payload.begin() + i, payload.begin() + i + 4, tag_sz);

        string tag(tag_sz);

        uint32_t taglength = read_32b(payload.begin() + i + 4);

        if (taglength % 8 != 0) {
            etiLog.log(warn, "Invalid tag length!");
            break;
        }
        taglength /= 8;

        length = taglength;

        vector<uint8_t> tag_value(taglength);
        copy(   payload.begin() + i+8,
                payload.begin() + i+8+taglength,
                tag_value.begin());

        bool tagsuccess = false;
        if (tag == "*ptr") {
            tagsuccess = decode_starptr(tag_value);
        }
        else if (tag == "deti") {
            tagsuccess = decode_deti(tag_value);
        }
        else if (tag.substr(0, 3) == "est") {
            uint8_t n = tag_sz[3];
            tagsuccess = decode_estn(tag_value, n);
        }
        else if (tag == "*dmy") {
            tagsuccess = decode_stardmy(tag_value);
        }
        else {
            etiLog.log(warn, "Unknown TAG %s", tag.c_str());
            break;
        }

        if (not tagsuccess) {
            etiLog.log(warn, "Error decoding TAG %s", tag.c_str());
            success = tagsuccess;
            break;
        }
    }

    return success;
}

bool ETIDecoder::decode_starptr(const vector<uint8_t> &value)
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

    m_eti_writer.update_protocol(protocol, major, minor);

    return true;
}

bool ETIDecoder::decode_deti(const vector<uint8_t> &value)
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

    m_eti_writer.update_err(stat);
    m_eti_writer.update_mnsc(mnsc);

    size_t i = 2 + 4;

    if (fc.atstf) {
        uint8_t utco = value[i];
        i++;

        uint32_t seconds = read_32b(value.begin() + i);
        i += 4;

        m_eti_writer.update_edi_time(utco, seconds);

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

        m_eti_writer.update_fic(fic);
    }

    if (rfudf) {
        uint32_t rfud = read_24b(value.begin() + i);

        // high 16 bits: RFU in LIDATA EOH
        // low 8 bits: RFU in TIST (not supported)
        m_eti_writer.update_rfu(rfud >> 8);
        if ((rfud & 0xFF) != 0xFF) {
            etiLog.level(warn) << "EDI: RFU in TIST not supported";
        }

        i += 3;
    }

    m_eti_writer.update_fc_data(fc);

    return true;
}

bool ETIDecoder::decode_estn(const vector<uint8_t> &value, uint8_t n)
{
    uint32_t sstc = read_24b(value.begin());

    eti_stc_data stc;

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

    m_eti_writer.add_subchannel(stc);

    return true;
}

bool ETIDecoder::decode_stardmy(const vector<uint8_t> &value)
{
    return true;
}

}
