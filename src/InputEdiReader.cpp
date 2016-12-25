/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#include "InputReader.h"
#include "PcDebug.h"

#include <stdexcept>
#include <memory>
#include <regex>
#include <sys/types.h>
#include <string.h>

using namespace std;

InputEdiReader::InputEdiReader() :
    m_writer(),
    m_decoder(m_writer),
    m_sock()
{
}

int InputEdiReader::Open(const std::string& uri)
{
    etiLog.level(info) << "Opening EDI :" << uri;

    const std::regex re_udp("udp://:([0-9]+)");
    std::smatch m;
    if (std::regex_match(uri, m, re_udp)) {
        m_port = std::stoi(m[1].str());

        etiLog.level(info) << "EDI port :" << m_port;
        return m_sock.reinit(m_port, "0.0.0.0");
    }

    return 1;
}

void InputEdiReader::rx_packet()
{
    const size_t packsize = 8192;
    UdpPacket packet(packsize);

    int ret = m_sock.receive(packet);
    if (ret == 0) {
        const auto &buf = packet.getBuffer();
        if (packet.getSize() == packsize) {
            fprintf(stderr, "Warning, possible UDP truncation\n");
        }

        m_decoder.push_packet(buf);
    }
    else {
        fprintf(stderr, "Socket error: %s\n", inetErrMsg);
    }
}

int InputEdiReader::GetNextFrame(void* buffer)
{
    vector<uint8_t> eti;
    while (eti.empty()) {
        rx_packet();
        eti = m_writer.getEtiFrame();
    }

    assert(eti.size() == 6144);
    copy(eti.begin(), eti.end(), reinterpret_cast<uint8_t*>(buffer));

    return 6144;
}

void InputEdiReader::PrintInfo()
{
    fprintf(stderr, "EDI Input: \n");
    fprintf(stderr, "     Port : %d\n", m_port);
}

