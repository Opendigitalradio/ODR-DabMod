/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2016
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

InputTcpReader::InputTcpReader()
{
    if ((m_sock = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        throw std::runtime_error("Can't create TCP socket");
    }
}

InputTcpReader::~InputTcpReader()
{
    if (m_sock != INVALID_SOCKET) {
        close(m_sock);
    }
}

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

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);

    hostent *host = gethostbyname(hostname.c_str());
    if (host) {
        addr.sin_addr = *(in_addr *)(host->h_addr);
    }
    else {
        std::stringstream ss;
        ss << "Could not resolve hostname " << hostname << ": " << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    if (connect(m_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::stringstream ss;
        ss << "Could not connect to " << hostname << ":" << port << " :" << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    m_uri = endpoint;
}

int InputTcpReader::GetNextFrame(void* buffer)
{
    uint8_t* buf = (uint8_t*)buffer;

    const size_t framesize = 6144;

    ssize_t r = recv(m_sock, buf, framesize, MSG_WAITALL);

    if (r == -1) {
        std::stringstream ss;
        ss << "Could not receive from socket :" << strerror(errno);
        throw std::runtime_error(ss.str());
    }

    return r;
}

void InputTcpReader::PrintInfo()
{
    fprintf(stderr, "Input TCP:\n");
    fprintf(stderr, "  Receiving from %s\n\n", m_uri.c_str());
}

