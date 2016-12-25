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
#include "ETIWriter.hpp"
#include "crc.h"
#include "Log.h"
#include <stdio.h>
#include <cassert>
#include <stdexcept>
#include <sstream>

namespace EdiDecoder {

using namespace std;

void ETIWriter::update_protocol(
        const std::string& proto,
        uint16_t major,
        uint16_t minor)
{
    m_proto_valid = (proto == "DETI" and major == 0 and minor == 0);

    if (not m_proto_valid) {
        throw std::invalid_argument("Wrong EDI protocol");
    }
}

void ETIWriter::reinit()
{
    m_proto_valid = false;
    m_fc_valid = false;
    m_fic.clear();
    m_etiFrame.clear();
    m_subchannels.clear();
}

void ETIWriter::update_err(uint8_t err)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update ERR before protocol");
    }
    m_err = err;
}

void ETIWriter::update_fc_data(const eti_fc_data& fc_data)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update FC before protocol");
    }

    m_fc_valid = false;
    m_fc = fc_data;

    if (not m_fc.ficf) {
        throw std::invalid_argument("FIC must be present");
    }

    if (m_fc.mid > 4) {
        throw std::invalid_argument("Invalid MID");
    }

    if (m_fc.fp > 7) {
        throw std::invalid_argument("Invalid FP");
    }

    m_fc_valid = true;
}

void ETIWriter::update_fic(const std::vector<uint8_t>& fic)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update FIC before protocol");
    }

    m_fic = fic;
}

void ETIWriter::update_edi_time(
        uint32_t utco,
        uint32_t seconds)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update time before protocol");
    }

    m_utco = utco;
    m_seconds = seconds;

    // TODO check validity
    m_time_valid = true;

}

void ETIWriter::update_mnsc(uint16_t mnsc)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update MNSC before protocol");
    }

    m_mnsc = mnsc;
}

void ETIWriter::add_subchannel(const eti_stc_data& stc)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot add subchannel before protocol");
    }

    m_subchannels.push_back(stc);

    if (m_subchannels.size() > 64) {
        throw std::invalid_argument("Too many subchannels");
    }

}

void ETIWriter::assemble()
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot assemble ETI before protocol");
    }

    if (not m_fc_valid) {
        throw std::logic_error("Cannot assemble ETI without FC");
    }

    if (m_fic.empty()) {
        throw std::logic_error("Cannot assemble ETI without FIC data");
    }

    // Accept zero subchannels, because of an edge-case that can happen
    // during reconfiguration. See ETS 300 799 Clause 5.3.3

    // TODO check time validity

    // ETS 300 799 Clause 5.3.2, but we don't support not having
    // a FIC
    if (    (m_fc.mid == 3 and m_fic.size() != 32 * 4) or
            (m_fc.mid != 3 and m_fic.size() != 24 * 4) ) {
        stringstream ss;
        ss << "Invalid FIC length " << m_fic.size() <<
            " for MID " << m_fc.mid;
        throw std::invalid_argument(ss.str());
    }


    std::vector<uint8_t> eti;
    eti.reserve(6144);

    eti.push_back(m_err);

    // FSYNC
    if (m_fc.fct() % 2 == 1) {
        eti.push_back(0xf8);
        eti.push_back(0xc5);
        eti.push_back(0x49);
    }
    else {
        eti.push_back(0x07);
        eti.push_back(0x3a);
        eti.push_back(0xb6);
    }

    // LIDATA
    // FC
    eti.push_back(m_fc.fct());

    const uint8_t NST = m_subchannels.size();

    eti.push_back((m_fc.ficf << 7) | NST);

    // We need to pack:
    //  FP 3 bits
    //  MID 2 bits
    //  FL 11 bits

    // FL: EN 300 799 5.3.6
    uint16_t FL = NST + 1 + m_fic.size();
    for (const auto& subch : m_subchannels) {
        FL += subch.mst.size();
    }

    const uint16_t fp_mid_fl = (m_fc.fp << 13) | (m_fc.mid << 11) | FL;

    eti.push_back(fp_mid_fl >> 8);
    eti.push_back(fp_mid_fl & 0xFF);

    // STC
    for (const auto& subch : m_subchannels) {
        eti.push_back( (subch.scid << 2) | (subch.sad & 0x300) );
        eti.push_back( subch.sad & 0xff );
        eti.push_back( (subch.tpl << 2) | ((subch.stl() & 0x300) >> 8) );
        eti.push_back( subch.stl() & 0xff );
    }

    // EOH
    // MNSC
    eti.push_back(m_mnsc >> 8);
    eti.push_back(m_mnsc & 0xFF);

    // CRC
    // Calculate CRC from eti[4] to current position
    uint16_t eti_crc = 0xFFFF;
    eti_crc = crc16(eti_crc, &eti[4], eti.size() - 4);
    eti_crc ^= 0xffff;
    eti.push_back(eti_crc >> 8);
    eti.push_back(eti_crc & 0xFF);

    const size_t mst_start = eti.size();
    // MST
    // FIC data
    copy(m_fic.begin(), m_fic.end(), back_inserter(eti));

    // Data stream
    for (const auto& subch : m_subchannels) {
        copy(subch.mst.begin(), subch.mst.end(), back_inserter(eti));
    }

    // EOF
    // CRC
    uint16_t mst_crc = 0xFFFF;
    mst_crc = crc16(mst_crc, &eti[mst_start], eti.size() - mst_start);
    mst_crc ^= 0xffff;
    eti.push_back(mst_crc >> 8);
    eti.push_back(mst_crc & 0xFF);

    // RFU
    eti.push_back(0xff);
    eti.push_back(0xff);

    // TIST
    eti.push_back(m_fc.tsta >> 24);
    eti.push_back((m_fc.tsta >> 16) & 0xFF);
    eti.push_back((m_fc.tsta >> 8) & 0xFF);
    eti.push_back(m_fc.tsta & 0xFF);

    if (eti.size() > 6144) {
        throw std::logic_error("ETI frame cannot be longer than 6144");
    }

    eti.resize(6144, 0x55);

    m_etiFrame = eti;

}

std::vector<uint8_t> ETIWriter::getEtiFrame()
{
    if (m_etiFrame.empty()) {
        return {};
    }

    vector<uint8_t> eti(move(m_etiFrame));
    reinit();

    return eti;
}

}

