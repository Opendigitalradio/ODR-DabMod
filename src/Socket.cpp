/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#include "Socket.h"
#include "Log.h"
#include <fcntl.h>

TCPSocket::TCPSocket()
{
    if ((m_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        throw std::runtime_error("Can't create TCP socket");
    }

#if defined(HAVE_SO_NOSIGPIPE)
    int val = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE,
                &val, sizeof(val)) < 0) {
        throw std::runtime_error("Can't set SO_NOSIGPIPE");
    }
#endif
}

TCPSocket::~TCPSocket()
{
    if (m_sock != -1) {
        ::close(m_sock);
    }
}

TCPSocket::TCPSocket(TCPSocket&& other)
{
    m_sock = other.m_sock;

    if (other.m_sock != -1) {
        other.m_sock = -1;
    }
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other)
{
    m_sock = other.m_sock;

    if (other.m_sock != -1) {
        other.m_sock = -1;
    }

    return *this;
}

bool TCPSocket::valid() const
{
    return m_sock != -1;
}

void TCPSocket::connect(const std::string& hostname, int port)
{
    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);

    hostent *host = gethostbyname(hostname.c_str());
    if (host) {
        addr.sin_addr = *(in_addr *)(host->h_addr);
    }
    else {
        std::string errstr(strerror(errno));
        throw std::runtime_error(
                "could not resolve hostname " +
                hostname + ":" + std::to_string(port) +
                " : " + errstr);
    }

    int ret = ::connect(m_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1 and errno != EINPROGRESS) {
        std::string errstr(strerror(errno));
        throw std::runtime_error(
                "could not connect to " +
                hostname + ":" + std::to_string(port) +
                " : " + errstr);
    }
}

void TCPSocket::listen(int port)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    const int reuse = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR,
                &reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("Can't reuse address for TCP socket");
    }

    if (::bind(m_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close();
        throw std::runtime_error("Can't bind TCP socket");
    }

    if (::listen(m_sock, 1) < 0) {
        close();
        m_sock = -1;
        throw std::runtime_error("Can't listen TCP socket");
    }

}

void TCPSocket::close()
{
    ::close(m_sock);
    m_sock = -1;
}

TCPSocket TCPSocket::accept_with_timeout(int timeout_ms, struct sockaddr_in *client)
{
    struct pollfd fds[1];
    fds[0].fd = m_sock;
    fds[0].events = POLLIN;

    int retval = poll(fds, 1, timeout_ms);

    if (retval == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP Socket accept error: " + errstr);
    }
    else if (retval > 0) {
        socklen_t client_len = sizeof(struct sockaddr_in);
        int sockfd = accept(m_sock, (struct sockaddr*)&client, &client_len);
        TCPSocket s(sockfd);
        return s;
    }
    else {
        TCPSocket s(-1);
        return s;
    }
}

ssize_t TCPSocket::sendall(const void *buffer, size_t buflen)
{
    uint8_t *buf = (uint8_t*)buffer;
    while (buflen > 0) {
        /* On Linux, the MSG_NOSIGNAL flag ensures that the process
         * would not receive a SIGPIPE and die.
         * Other systems have SO_NOSIGPIPE set on the socket for the
         * same effect. */
#if defined(HAVE_MSG_NOSIGNAL)
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        ssize_t sent = ::send(m_sock, buf, buflen, flags);
        if (sent < 0) {
            return -1;
        }
        else {
            buf += sent;
            buflen -= sent;
        }
    }
    return buflen;
}

ssize_t TCPSocket::recv(void *buffer, size_t length, int flags)
{
    ssize_t ret = ::recv(m_sock, buffer, length, flags);
    if (ret == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP receive error: " + errstr);
    }
    return ret;
}

ssize_t TCPSocket::recv(void *buffer, size_t length, int flags, int timeout_ms)
{
    struct pollfd fds[1];
    fds[0].fd = m_sock;
    fds[0].events = POLLIN;

    int retval = poll(fds, 1, timeout_ms);

    if (retval == -1 and errno == EINTR) {
        throw Interrupted();
    }
    else if (retval == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP receive with poll() error: " + errstr);
    }
    else if (retval > 0 and (fds[0].revents | POLLIN)) {
        ssize_t ret = ::recv(m_sock, buffer, length, flags);
        if (ret == -1) {
            if (errno == ECONNREFUSED) {
                return 0;
            }
            std::string errstr(strerror(errno));
            throw std::runtime_error("TCP receive after poll() error: " + errstr);
        }
        return ret;
    }
    else {
        throw Timeout();
    }
}

TCPSocket::TCPSocket(int sockfd) {
    m_sock = sockfd;
}

void TCPClient::connect(const std::string& hostname, int port)
{
    m_hostname = hostname;
    m_port = port;
    reconnect();
}

ssize_t TCPClient::recv(void *buffer, size_t length, int flags, int timeout_ms)
{
    try {
        ssize_t ret = m_sock.recv(buffer, length, flags, timeout_ms);

        if (ret == 0) {
            m_sock.close();

            TCPSocket newsock;
            m_sock = std::move(newsock);
            reconnect();
        }

        return ret;
    }
    catch (const TCPSocket::Interrupted&) {
        return -1;
    }
    catch (const TCPSocket::Timeout&) {
        return 0;
    }

    return 0;
}

void TCPClient::reconnect()
{
    int flags = fcntl(m_sock.m_sock, F_GETFL);
    if (fcntl(m_sock.m_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set O_NONBLOCK: " + errstr);
    }

    m_sock.connect(m_hostname, m_port);
}
