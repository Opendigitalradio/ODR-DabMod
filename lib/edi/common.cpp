/*
   Copyright (C) 2020
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
#include <algorithm>
#include <sstream>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cctype>

namespace EdiDecoder {

using namespace std;

bool frame_timestamp_t::is_valid() const
{
    return tsta != 0xFFFFFF and seconds != 0;
}

string frame_timestamp_t::to_string() const
{
    const time_t seconds_in_unix_epoch = to_unix_epoch();

    stringstream ss;
    if (is_valid()) {
        ss << "Timestamp: ";
    }
    else {
        ss << "Timestamp not valid: ";
    }

    char timestr[100];
    if (std::strftime(timestr, sizeof(timestr), "%Y-%m-%dZ%H:%M:%S", std::gmtime(&seconds_in_unix_epoch))) {
        ss << timestr << " + " << ((double)tsta / 16384000.0);
    }
    else {
        ss << "unknown";
    }
    return ss.str();
}

time_t frame_timestamp_t::to_unix_epoch() const
{
    // EDI epoch: 2000-01-01T00:00:00Z
    // Convert using
    // TZ=UTC python -c 'import datetime; print(datetime.datetime(2000,1,1,0,0,0,0).strftime("%s"))'
    return 946684800 + seconds - utco;
}

double frame_timestamp_t::diff_s(const frame_timestamp_t& other) const
{
    const double lhs = (double)seconds + (tsta / 16384000.0);
    const double rhs = (double)other.seconds + (other.tsta / 16384000.0);
    return lhs - rhs;
}

frame_timestamp_t& frame_timestamp_t::operator+=(const std::chrono::milliseconds& ms)
{
    tsta += (ms.count() % 1000) << 14; // Shift ms by 14 to Timestamp level 2
    if (tsta > 0xf9FFff) {
        tsta -= 0xfa0000; // Substract 16384000, corresponding to one second
        seconds += 1;
    }

    seconds += (ms.count() / 1000);

    return *this;
}

frame_timestamp_t frame_timestamp_t::from_unix_epoch(std::time_t time, uint32_t tai_utc_offset, uint32_t tsta)
{
    frame_timestamp_t ts;

    const std::time_t posix_timestamp_1_jan_2000 = 946684800;

    ts.utco = tai_utc_offset - 32;
    ts.seconds = time - posix_timestamp_1_jan_2000 + ts.utco;
    ts.tsta = tsta;
    return ts;
}

std::chrono::system_clock::time_point frame_timestamp_t::to_system_clock() const
{
    auto ts = chrono::system_clock::from_time_t(to_unix_epoch());

    // PPS offset in seconds = tsta / 16384000
    // We cannot use nanosecond resolution because not all platforms use a
    // system_clock that has nanosecond precision. It's not really important,
    // as this function is only used for debugging.
    ts += chrono::microseconds(std::lrint(tsta / 16.384));

    return ts;
}

std::string tag_name_to_human_readable(const tag_name_t& name)
{
    std::string s;
    for (const uint8_t c : name) {
        if (isprint(c)) {
            s += (char)c;
        }
        else {
            char escaped[5];
            snprintf(escaped, 5, "\\x%02x", c);
            s += escaped;
        }
    }
    return s;
}

TagDispatcher::TagDispatcher(std::function<void()>&& af_packet_completed) :
    m_af_packet_completed(std::move(af_packet_completed)),
    m_afpacket_handler([](std::vector<uint8_t>&& /*ignore*/){})
{
}

void TagDispatcher::set_verbose(bool verbose)
{
    m_pft.setVerbose(verbose);
}

void TagDispatcher::push_bytes(const vector<uint8_t> &buf)
{
    if (buf.empty()) {
        m_input_data.clear();
        m_last_sequences.seq_valid = false;
        return;
    }

    copy(buf.begin(), buf.end(), back_inserter(m_input_data));

    while (m_input_data.size() > 2) {
        if (m_input_data[0] == 'A' and m_input_data[1] == 'F') {
            const auto r = decode_afpacket(m_input_data);
            bool leave_loop = false;
            switch (r.st) {
                case decode_state_e::Ok:
                    m_last_sequences.pseq_valid = false;
                    m_af_packet_completed();
                    break;
                case decode_state_e::MissingData:
                    /* Continue filling buffer */
                    leave_loop = true;
                    break;
                case decode_state_e::Error:
                    m_last_sequences.pseq_valid = false;
                    leave_loop = true;
                    break;
            }

            if (r.num_bytes_consumed) {
                vector<uint8_t> remaining_data;
                copy(m_input_data.begin() + r.num_bytes_consumed,
                        m_input_data.end(),
                        back_inserter(remaining_data));
                m_input_data = remaining_data;
            }

            if (leave_loop) {
                break;
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
            if (not af.af_packet.empty()) {
                const auto r = decode_afpacket(af.af_packet);

                switch (r.st) {
                    case decode_state_e::Ok:
                        m_last_sequences.pseq = af.pseq;
                        m_last_sequences.pseq_valid = true;
                        m_af_packet_completed();
                        break;
                    case decode_state_e::MissingData:
                        etiLog.level(error) << "ETI MissingData on PFT push_bytes";
                        m_last_sequences.pseq_valid = false;
                        break;
                    case decode_state_e::Error:
                        m_last_sequences.pseq_valid = false;
                        break;
                }
            }
        }
        else {
            etiLog.log(warn, "Unknown 0x%02x!", *m_input_data.data());
            m_input_data.erase(m_input_data.begin());
        }
    }
}

void TagDispatcher::push_packet(const Packet &packet)
{
    auto& buf = packet.buf;

    if (buf.size() < 2) {
        throw std::invalid_argument("Not enough bytes to read EDI packet header");
    }

    if (buf[0] == 'A' and buf[1] == 'F') {
        const auto r = decode_afpacket(buf);
        m_last_sequences.pseq_valid = false;

        if (r.st == decode_state_e::Ok) {
            m_af_packet_completed();
        }

    }
    else if (buf[0] == 'P' and buf[1] == 'F') {
        PFT::Fragment fragment;
        fragment.loadData(buf, packet.received_on_port);

        if (fragment.isValid()) {
            m_pft.pushPFTFrag(fragment);
        }

        auto af = m_pft.getNextAFPacket();
        if (not af.af_packet.empty()) {
            const auto r = decode_afpacket(af.af_packet);

            if (r.st == decode_state_e::Ok) {
                m_last_sequences.pseq = af.pseq;
                m_last_sequences.pseq_valid = true;
                m_af_packet_completed();
            }
        }
    }
    else {
        std::stringstream ss;
        ss << "Unknown EDI packet " << std::hex << (int)buf[0] << " " << (int)buf[1];
        m_ignored_tags.clear();
        throw invalid_argument(ss.str());
    }
}

void TagDispatcher::setMaxDelay(int num_af_packets)
{
    m_pft.setMaxDelay(num_af_packets);
}


TagDispatcher::decode_result_t TagDispatcher::decode_afpacket(
        const std::vector<uint8_t> &input_data)
{
    if (input_data.size() < AFPACKET_HEADER_LEN) {
        return {decode_state_e::MissingData, 0};
    }

    // read length from packet
    uint32_t taglength = read_32b(input_data.begin() + 2);
    uint16_t seq = read_16b(input_data.begin() + 6);

    const size_t crclength = 2;
    if (input_data.size() < AFPACKET_HEADER_LEN + taglength + crclength) {
        return {decode_state_e::MissingData, 0};
    }

    // SEQ wraps at 0xFFFF, unsigned integer overflow is intentional
    if (m_last_sequences.seq_valid) {
        const uint16_t expected_seq = m_last_sequences.seq + 1;
        if (expected_seq != seq) {
            etiLog.level(warn) << "EDI AF Packet sequence error, " << seq;
            m_ignored_tags.clear();
        }
    }
    else {
        etiLog.level(info) << "EDI AF Packet initial sequence number: " << seq;
        m_last_sequences.seq_valid = true;
    }
    m_last_sequences.seq = seq;

    const size_t crclen = 2;
    bool has_crc = (input_data[8] & 0x80) ? true : false;
    uint8_t major_revision = (input_data[8] & 0x70) >> 4;
    uint8_t minor_revision = input_data[8] & 0x0F;
    if (major_revision != 1 or minor_revision != 0) {
        etiLog.level(warn) << "EDI AF Packet has wrong revision " <<
                (int)major_revision << "." << (int)minor_revision;
    }

    if (not has_crc) {
        etiLog.level(warn) << "AF packet not supported, has no CRC";
        return {decode_state_e::Error, AFPACKET_HEADER_LEN + taglength};
    }
    uint8_t pt = input_data[9];
    if (pt != 'T') {
        // only support Tag
        return {decode_state_e::Error, AFPACKET_HEADER_LEN + taglength + crclen};
    }

    uint16_t crc = 0xffff;
    for (size_t i = 0; i < AFPACKET_HEADER_LEN + taglength; i++) {
        crc = crc16(crc, &input_data[i], 1);
    }
    crc ^= 0xffff;

    uint16_t packet_crc = read_16b(input_data.begin() + AFPACKET_HEADER_LEN + taglength);

    if (packet_crc != crc) {
        etiLog.level(warn) << "AF Packet crc wrong";
        return {decode_state_e::Error, AFPACKET_HEADER_LEN + taglength + crclen};
    }
    else {
        vector<uint8_t> afpacket(AFPACKET_HEADER_LEN + taglength + crclen);
        copy(input_data.begin(),
                input_data.begin() + AFPACKET_HEADER_LEN + taglength + crclen,
                afpacket.begin());
        m_afpacket_handler(std::move(afpacket));

        vector<uint8_t> payload(taglength);
        copy(input_data.begin() + AFPACKET_HEADER_LEN,
                input_data.begin() + AFPACKET_HEADER_LEN + taglength,
                payload.begin());

        auto result = decode_tagpacket(payload) ? decode_state_e::Ok : decode_state_e::Error;
        return {result, AFPACKET_HEADER_LEN + taglength + crclen};
    }
}

void TagDispatcher::register_tag(const std::string& tag, tag_handler&& h)
{
    m_handlers[tag] = std::move(h);
}

void TagDispatcher::register_afpacket_handler(afpacket_handler&& h)
{
    m_afpacket_handler = std::move(h);
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
            etiLog.log(warn, "Invalid EDI tag length, not multiple of 8!");
            break;
        }
        taglength /= 8;

        length = taglength;

        const size_t calculated_length = i + 8 + taglength;
        if (calculated_length > payload.size()) {
            etiLog.log(warn, "Invalid EDI tag length: tag larger %zu than tagpacket %zu!",
                    calculated_length, payload.size());
            break;
        }

        const array<uint8_t, 4> tag_name({
               (uint8_t)tag_sz[0], (uint8_t)tag_sz[1], (uint8_t)tag_sz[2], (uint8_t)tag_sz[3]
               });
        vector<uint8_t> tag_value(taglength);
        copy(   payload.begin() + i+8,
                payload.begin() + i+8+taglength,
                tag_value.begin());

        bool tagsuccess = true;
        bool found = false;
        for (auto tag_handler : m_handlers) {
            if (    (tag_handler.first.size() == 4 and tag == tag_handler.first) or
                    (tag_handler.first.size() == 3 and tag.substr(0, 3) == tag_handler.first) or
                    (tag_handler.first.size() == 2 and tag.substr(0, 2) == tag_handler.first) or
                    (tag_handler.first.size() == 1 and tag.substr(0, 1) == tag_handler.first)) {
                found = true;
                tagsuccess &= tag_handler.second(tag_value, tag_name);
            }
        }

        if (not found) {
            if (std::find(m_ignored_tags.begin(), m_ignored_tags.end(), tag) == m_ignored_tags.end()) {
                etiLog.log(warn, "Ignoring unknown TAG %s", tag.c_str());
                m_ignored_tags.push_back(tag);
            }
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

odr_version_data parse_odr_version_data(const std::vector<uint8_t>& data)
{
    if (data.size() < sizeof(uint32_t)) {
        return {};
    }

    const size_t versionstr_length = data.size() - sizeof(uint32_t);
    string version(data.begin(), data.begin() + versionstr_length);
    uint32_t uptime_s = read_32b(data.begin() + versionstr_length);

    return {version, uptime_s};
}

}
