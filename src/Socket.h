/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

class TCPSocket {
    public:
        TCPSocket();
        ~TCPSocket();
        TCPSocket(const TCPSocket& other) = delete;
        TCPSocket& operator=(const TCPSocket& other) = delete;
        TCPSocket(TCPSocket&& other);
        TCPSocket& operator=(TCPSocket&& other);

        bool valid(void) const;
        void connect(const std::string& hostname, int port);
        void listen(int port);
        void close(void);

        /* throws a runtime_error on failure, an invalid socket on timeout */
        TCPSocket accept_with_timeout(int timeout_ms, struct sockaddr_in *client);

        /* returns -1 on error */
        ssize_t sendall(const void *buffer, size_t buflen);

        /* Returns number of bytes read, 0 on disconnect. Throws a
         * runtime_error on error */
        ssize_t recv(void *buffer, size_t length, int flags);

        class Timeout {};
        class Interrupted {};
        /* Returns number of bytes read, 0 on disconnect or refused connection.
         * Throws a Timeout on timeout, Interrupted on EINTR, a runtime_error
         * on error
         */
        ssize_t recv(void *buffer, size_t length, int flags, int timeout_ms);

    private:
        explicit TCPSocket(int sockfd);
        int m_sock = -1;

        friend class TCPClient;
};

/* Implement a TCP receiver that auto-reconnects on errors */
class TCPClient {
    public:
        void connect(const std::string& hostname, int port);

        /* Returns numer of bytes read, 0 on auto-reconnect, -1
         * on interruption.
         * Throws a runtime_error on error */
        ssize_t recv(void *buffer, size_t length, int flags, int timeout_ms);

    private:
        void reconnect(void);
        TCPSocket m_sock;
        std::string m_hostname;
        int m_port;
};

