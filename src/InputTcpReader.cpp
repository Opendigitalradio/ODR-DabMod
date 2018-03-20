/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "InputReader.h"
#include "PcDebug.h"
#include "Utils.h"
#include <unistd.h>
#include <errno.h>

void InputTcpReader::Open(const std::string& endpoint)
{
    std::string hostname;
    if (endpoint.compare(0, 6, "tcp://") == 0) {
        hostname = endpoint.substr(6, std::string::npos);
    }
    else {
        hostname = endpoint;
    }

    size_t colon_pos = hostname.find(":");
    if (colon_pos == std::string::npos) {
        std::stringstream ss;
        ss << "Could not parse TCP endpoint " << endpoint;
        throw std::runtime_error(ss.str());
    }

    long port = strtol(hostname.c_str() + colon_pos + 1, NULL, 10);
    if (errno == ERANGE) {
        std::stringstream ss;
        ss << "Could not parse port in TCP endpoint " << endpoint;
        throw std::runtime_error(ss.str());
    }

    hostname = hostname.substr(0, colon_pos);

    m_tcpclient.connect(hostname, port);

    m_uri = endpoint;
}

int InputTcpReader::GetNextFrame(void* buffer)
{
    uint8_t* buf = (uint8_t*)buffer;

    const size_t framesize = 6144;
    const int timeout_ms = 8000;

    ssize_t ret = m_tcpclient.recv(buf, framesize, MSG_WAITALL, timeout_ms);

    if (ret == 0) {
        etiLog.level(debug) << "TCP input auto reconnect";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return ret;
}

std::string InputTcpReader::GetPrintableInfo() const
{
    return "Input TCP: Receiving from " + m_uri;
}

