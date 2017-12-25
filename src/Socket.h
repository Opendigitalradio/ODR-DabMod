/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   Abstraction for sockets.
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <unistd.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <poll.h>

class TCPSocket {
    public:
        TCPSocket() {
            if ((m_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                throw std::runtime_error("Can't create TCP socket");
            }

#if defined(HAVE_SO_NOSIGPIPE)
            int val = 1;
            if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val))
                    == SOCKET_ERROR) {
                throw std::runtime_error("Can't set SO_NOSIGPIPE");
            }
#endif
        }

        ~TCPSocket() {
            if (m_sock != -1) {
                ::close(m_sock);
            }
        }

        TCPSocket(const TCPSocket& other) = delete;
        TCPSocket& operator=(const TCPSocket& other) = delete;
        TCPSocket(TCPSocket&& other) {
            m_sock = other.m_sock;

            if (other.m_sock != -1) {
                other.m_sock = -1;
            }
        }

        TCPSocket& operator=(TCPSocket&& other)
        {
            m_sock = other.m_sock;

            if (other.m_sock != -1) {
                other.m_sock = -1;
            }

            return *this;
        }

        bool valid(void) const {
            return m_sock != -1;
        }

        void listen(int port) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            const int reuse = 1;
            if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
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

        void close(void) {
            ::close(m_sock);
            m_sock = -1;
        }

        TCPSocket accept_with_timeout(int timeout_ms, struct sockaddr_in *client)
        {
            struct pollfd fds[1];
            fds[0].fd = m_sock;
            fds[0].events = POLLIN | POLLOUT;

            int retval = poll(fds, 1, timeout_ms);

            if (retval == -1) {
                throw std::runtime_error("TCP Socket accept error: " + std::to_string(errno));
            }
            else if (retval) {
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

        ssize_t sendall(const void *buffer, size_t buflen)
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

        ssize_t recv(void *buffer, size_t length, int flags)
        {
            return ::recv(m_sock, buffer, length, flags);
        }

    private:
        explicit TCPSocket(int sockfd) {
            m_sock = sockfd;
        }

        int m_sock = -1;
};

