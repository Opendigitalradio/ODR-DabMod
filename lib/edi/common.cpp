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
#include "common.hpp"
#include "buffer_unpack.hpp"
#include "Log.h"
#include "crc.h"
#include <iomanip>
#include <sstream>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace EdiDecoder {

using namespace std;

bool frame_timestamp_t::valid() const
{
    return tsta != 0xFFFFFF;
}

string frame_timestamp_t::to_string() const
{
    const time_t seconds_in_unix_epoch = to_unix_epoch();

    stringstream ss;
    if (valid()) {
        ss << "Timestamp: ";
    }
    else {
        ss << "Timestamp not valid: ";
    }
    ss << std::put_time(std::gmtime(&seconds_in_unix_epoch), "%c %Z") <<
        " + " << ((double)tsta / 16384000.0);
    return ss.str();
}

time_t frame_timestamp_t::to_unix_epoch() const
{
    // EDI epoch: 2000-01-01T00:00:00Z
    // Convert using
    // TZ=UTC python -c 'import datetime; print(datetime.datetime(2000,1,1,0,0,0,0).strftime("%s"))'
    return 946684800 + seconds - utco;
}

std::chrono::system_clock::time_point frame_timestamp_t::to_system_clock() const
{
    auto ts = chrono::system_clock::from_time_t(to_unix_epoch());

    // PPS offset in seconds = tsta / 16384000
    ts += chrono::nanoseconds(std::lrint(tsta / 0.016384));

    return ts;
}


TagDispatcher::TagDispatcher(
        std::function<void()>&& af_packet_completed, bool verbose) :
    m_af_packet_completed(move(af_packet_completed))
{
    m_pft.setVerbose(verbose);
}

void TagDispatcher::push_bytes(const vector<uint8_t> &buf)
{
    copy(buf.begin(), buf.end(), back_inserter(m_input_data));

    while (m_input_data.size() > 2) {
        if (m_input_data[0] == 'A' and m_input_data[1] == 'F') {
            const decode_state_t st = decode_afpacket(m_input_data);

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
                m_af_packet_completed();
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
                    m_af_packet_completed();
                }
            }
        }
        else {
            etiLog.log(warn,"Unknown %c!", *m_input_data.data());
            m_input_data.erase(m_input_data.begin());
        }
    }
}

void TagDispatcher::push_packet(const vector<uint8_t> &buf)
{
    if (buf.size() < 2) {
        throw std::invalid_argument("Not enough bytes to read EDI packet header");
    }

    if (buf[0] == 'A' and buf[1] == 'F') {
        const decode_state_t st = decode_afpacket(buf);

        if (st.complete) {
            m_af_packet_completed();
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
            const decode_state_t st = decode_afpacket(af);

            if (st.complete) {
                m_af_packet_completed();
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

void TagDispatcher::setMaxDelay(int num_af_packets)
{
    m_pft.setMaxDelay(num_af_packets);
}


#define AFPACKET_HEADER_LEN 10 // includes SYNC
decode_state_t TagDispatcher::decode_afpacket(
        const std::vector<uint8_t> &input_data)
{
    if (input_data.size() < AFPACKET_HEADER_LEN) {
        return {false, 0};
    }

    // read length from packet
    uint32_t taglength = read_32b(input_data.begin() + 2);
    uint16_t seq = read_16b(input_data.begin() + 6);

    const size_t crclength = 2;
    if (input_data.size() < AFPACKET_HEADER_LEN + taglength + crclength) {
        return {false, 0};
    }

    if (m_last_seq + 1 != seq) {
        etiLog.level(warn) << "EDI AF Packet sequence error, " << seq;
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

void TagDispatcher::register_tag(const std::string& tag, tag_handler&& h)
{
    m_handlers[tag] = move(h);
}


bool TagDispatcher::decode_tagpacket(const vector<uint8_t> &payload)
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
        bool found = false;
        for (auto tag_handler : m_handlers) {
            if (tag_handler.first.size() == 4 and tag_handler.first == tag) {
                found = true;
                tagsuccess = tag_handler.second(tag_value, 0);
            }
            else if (tag_handler.first.size() == 3 and
                    tag.substr(0, 3) == tag_handler.first) {
                found = true;
                uint8_t n = tag_sz[3];
                tagsuccess = tag_handler.second(tag_value, n);
            }
            else if (tag_handler.first.size() == 2 and
                    tag.substr(0, 2) == tag_handler.first) {
                found = true;
                uint16_t n = 0;
                n = (uint16_t)(tag_sz[2]) << 8;
                n |= (uint16_t)(tag_sz[3]);
                tagsuccess = tag_handler.second(tag_value, n);
            }
        }

        if (not found) {
            etiLog.log(warn, "Ignoring unknown TAG %s", tag.c_str());
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

}
