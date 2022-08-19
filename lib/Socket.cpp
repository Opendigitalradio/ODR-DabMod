/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2022
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
   */
/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Socket.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>

namespace Socket {

using namespace std;

void InetAddress::resolveUdpDestination(const std::string& destination, int port)
{
    char service[NI_MAXSERV];
    snprintf(service, NI_MAXSERV-1, "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo *result, *rp;
    int s = getaddrinfo(destination.c_str(), service, &hints, &result);
    if (s != 0) {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(s));
    }

    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        // Take the first result
        memcpy(&addr, rp->ai_addr, rp->ai_addrlen);
        break;
    }

    freeaddrinfo(result);

    if (rp == nullptr) {
        throw runtime_error("Could not resolve");
    }
}

string InetAddress::to_string() const
{
    char received_from_str[64] = {};
    sockaddr *addr = reinterpret_cast<sockaddr*>(&addr);
    const char* ret = inet_ntop(AF_INET, addr, received_from_str, 63);

    if (ret == nullptr) {
        throw invalid_argument(string("Error converting InetAddress") + strerror(errno));
    }
    return ret;
}

UDPPacket::UDPPacket() { }

UDPPacket::UDPPacket(size_t initSize) :
    buffer(initSize),
    address()
{ }


UDPSocket::UDPSocket()
{
    reinit(0, "");
}

UDPSocket::UDPSocket(int port)
{
    reinit(port, "");
}

UDPSocket::UDPSocket(int port, const std::string& name)
{
    reinit(port, name);
}

UDPSocket::UDPSocket(UDPSocket&& other)
{
    m_sock = other.m_sock;
    m_port = other.m_port;
    other.m_port = 0;
    other.m_sock = INVALID_SOCKET;
}

const UDPSocket& UDPSocket::operator=(UDPSocket&& other)
{
    m_sock = other.m_sock;
    m_port = other.m_port;
    other.m_port = 0;
    other.m_sock = INVALID_SOCKET;
    return *this;
}

void UDPSocket::setBlocking(bool block)
{
    int res = fcntl(m_sock, F_SETFL, block ? 0 : O_NONBLOCK);
    if (res == -1) {
        throw runtime_error(string("Can't change blocking state of socket: ") + strerror(errno));
    }
}

void UDPSocket::reinit(int port)
{
    return reinit(port, "");
}

void UDPSocket::reinit(int port, const std::string& name)
{
    if (m_sock != INVALID_SOCKET) {
        ::close(m_sock);
    }

    m_port = port;

    if (port == 0) {
        // No need to bind to a given port, creating the
        // socket is enough
        m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
        return;
    }

    char service[NI_MAXSERV];
    snprintf(service, NI_MAXSERV-1, "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    struct addrinfo *result, *rp;
    int s = getaddrinfo(name.empty() ? nullptr : name.c_str(),
            port == 0 ? nullptr : service,
            &hints, &result);
    if (s != 0) {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(s));
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2).
       If socket(2) (or bind(2)) fails, we (close the socket
       and) try the next address. */
    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        int sfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        if (::bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            m_sock = sfd;
            break;
        }

        ::close(sfd);
    }

    freeaddrinfo(result);

    if (rp == nullptr) {
        throw runtime_error("Could not bind");
    }
}

void UDPSocket::close()
{
    if (m_sock != INVALID_SOCKET) {
        ::close(m_sock);
    }

    m_sock = INVALID_SOCKET;
}

UDPSocket::~UDPSocket()
{
    if (m_sock != INVALID_SOCKET) {
        ::close(m_sock);
    }
}


UDPPacket UDPSocket::receive(size_t max_size)
{
    UDPPacket packet(max_size);
    socklen_t addrSize;
    addrSize = sizeof(*packet.address.as_sockaddr());
    ssize_t ret = recvfrom(m_sock,
            packet.buffer.data(),
            packet.buffer.size(),
            0,
            packet.address.as_sockaddr(),
            &addrSize);

    if (ret == SOCKET_ERROR) {
        packet.buffer.resize(0);

        // This suppresses the -Wlogical-op warning
#if EAGAIN == EWOULDBLOCK
        if (errno == EAGAIN)
#else
        if (errno == EAGAIN or errno == EWOULDBLOCK)
#endif
        {
            return 0;
        }
        throw runtime_error(string("Can't receive data: ") + strerror(errno));
    }

    packet.buffer.resize(ret);
    return packet;
}

void UDPSocket::send(UDPPacket& packet)
{
    const int ret = sendto(m_sock, packet.buffer.data(), packet.buffer.size(), 0,
            packet.address.as_sockaddr(), sizeof(*packet.address.as_sockaddr()));
    if (ret == SOCKET_ERROR && errno != ECONNREFUSED) {
        throw runtime_error(string("Can't send UDP packet: ") + strerror(errno));
    }
}


void UDPSocket::send(const std::vector<uint8_t>& data, InetAddress destination)
{
    const int ret = sendto(m_sock, data.data(), data.size(), 0,
            destination.as_sockaddr(), sizeof(*destination.as_sockaddr()));
    if (ret == SOCKET_ERROR && errno != ECONNREFUSED) {
        throw runtime_error(string("Can't send UDP packet: ") + strerror(errno));
    }
}

void UDPSocket::send(const std::string& data, InetAddress destination)
{
    const int ret = sendto(m_sock, data.data(), data.size(), 0,
            destination.as_sockaddr(), sizeof(*destination.as_sockaddr()));
    if (ret == SOCKET_ERROR && errno != ECONNREFUSED) {
        throw runtime_error(string("Can't send UDP packet: ") + strerror(errno));
    }
}

void UDPSocket::joinGroup(const char* groupname, const char* if_addr)
{
    ip_mreqn group;
    if ((group.imr_multiaddr.s_addr = inet_addr(groupname)) == INADDR_NONE) {
        throw runtime_error("Cannot convert multicast group name");
    }
    if (!IN_MULTICAST(ntohl(group.imr_multiaddr.s_addr))) {
        throw runtime_error("Group name is not a multicast address");
    }

    if (if_addr) {
        group.imr_address.s_addr = inet_addr(if_addr);
    }
    else {
        group.imr_address.s_addr = htons(INADDR_ANY);
    }
    group.imr_ifindex = 0;
    if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group))
            == SOCKET_ERROR) {
        throw runtime_error(string("Can't join multicast group") + strerror(errno));
    }
}

void UDPSocket::setMulticastSource(const char* source_addr)
{
    struct in_addr addr;
    if (inet_aton(source_addr, &addr) == 0) {
        throw runtime_error(string("Can't parse source address") + strerror(errno));
    }

    if (setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr))
            == SOCKET_ERROR) {
        throw runtime_error(string("Can't set source address") + strerror(errno));
    }
}

void UDPSocket::setMulticastTTL(int ttl)
{
    if (setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))
            == SOCKET_ERROR) {
        throw runtime_error(string("Can't set multicast ttl") + strerror(errno));
    }
}

SOCKET UDPSocket::getNativeSocket() const
{
    return m_sock;
}

int UDPSocket::getPort() const
{
    return m_port;
}

void UDPReceiver::add_receive_port(int port, const string& bindto, const string& mcastaddr) {
    UDPSocket sock;

    if (IN_MULTICAST(ntohl(inet_addr(mcastaddr.c_str())))) {
        sock.reinit(port, mcastaddr);
        sock.setMulticastSource(bindto.c_str());
        sock.joinGroup(mcastaddr.c_str(), bindto.c_str());
    }
    else {
        sock.reinit(port, bindto);
    }

    m_sockets.push_back(move(sock));
}

vector<UDPReceiver::ReceivedPacket> UDPReceiver::receive(int timeout_ms)
{
    constexpr size_t MAX_FDS = 64;
    struct pollfd fds[MAX_FDS];
    if (m_sockets.size() > MAX_FDS) {
        throw std::runtime_error("UDPReceiver only supports up to 64 ports");
    }

    for (size_t i = 0; i < m_sockets.size(); i++) {
        fds[i].fd = m_sockets[i].getNativeSocket();
        fds[i].events = POLLIN;
    }

    int retval = poll(fds, m_sockets.size(), timeout_ms);

    if (retval == -1 and errno == EINTR) {
        throw Interrupted();
    }
    else if (retval == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("UDP receive with poll() error: " + errstr);
    }
    else if (retval > 0) {
        vector<ReceivedPacket> received;

        for (size_t i = 0; i < m_sockets.size(); i++) {
            if (fds[i].revents & POLLIN) {
                auto p = m_sockets[i].receive(2048); // This is larger than the usual MTU
                ReceivedPacket rp;
                rp.packetdata = move(p.buffer);
                rp.received_from = move(p.address);
                rp.port_received_on = m_sockets[i].getPort();
                received.push_back(move(rp));
            }
        }

        return received;
    }
    else {
        throw Timeout();
    }
}


TCPSocket::TCPSocket()
{
}

TCPSocket::~TCPSocket()
{
    if (m_sock != -1) {
        ::close(m_sock);
    }
}

TCPSocket::TCPSocket(TCPSocket&& other) :
    m_sock(other.m_sock),
    m_remote_address(move(other.m_remote_address))
{
    if (other.m_sock != -1) {
        other.m_sock = -1;
    }
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other)
{
    swap(m_remote_address, other.m_remote_address);

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

void TCPSocket::connect(const std::string& hostname, int port, int timeout_ms)
{
    if (m_sock != INVALID_SOCKET) {
        throw std::logic_error("You may only connect an invalid TCPSocket");
    }

    char service[NI_MAXSERV];
    snprintf(service, NI_MAXSERV-1, "%d", port);

    /* Obtain address(es) matching host/port */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo *result, *rp;
    int s = getaddrinfo(hostname.c_str(), service, &hints, &result);
    if (s != 0) {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(s));
    }

    int flags = 0;

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        int sfd = ::socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;

        flags = fcntl(sfd, F_GETFL);
        if (flags == -1) {
            std::string errstr(strerror(errno));
            throw std::runtime_error("TCP: Could not get socket flags: " + errstr);
        }

        if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::string errstr(strerror(errno));
            throw std::runtime_error("TCP: Could not set O_NONBLOCK: " + errstr);
        }

        int ret = ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            m_sock = sfd;
            break;
        }
        if (ret == -1 and errno == EINPROGRESS) {
            m_sock = sfd;
            struct pollfd fds[1];
            fds[0].fd = m_sock;
            fds[0].events = POLLOUT;

            int retval = poll(fds, 1, timeout_ms);

            if (retval == -1) {
                std::string errstr(strerror(errno));
                ::close(m_sock);
                freeaddrinfo(result);
                throw runtime_error("TCP: connect error on poll: " + errstr);
            }
            else if (retval > 0) {
                int so_error = 0;
                socklen_t len = sizeof(so_error);

                if (getsockopt(m_sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1) {
                    std::string errstr(strerror(errno));
                    ::close(m_sock);
                    freeaddrinfo(result);
                    throw runtime_error("TCP: getsockopt error connect: " + errstr);
                }

                if (so_error == 0) {
                    break;
                }
            }
            else {
                ::close(m_sock);
                freeaddrinfo(result);
                throw runtime_error("Timeout on connect");
            }
            break;
        }

        ::close(sfd);
    }

    if (m_sock != INVALID_SOCKET) {
#if defined(HAVE_SO_NOSIGPIPE)
        int val = 1;
        if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val))
                == SOCKET_ERROR) {
            throw runtime_error("Can't set SO_NOSIGPIPE");
        }
#endif
    }

    // Don't keep the socket blocking
    if (fcntl(m_sock, F_SETFL, flags) == -1) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set O_NONBLOCK: " + errstr);
    }

    freeaddrinfo(result);

    if (rp == nullptr) {
        throw runtime_error("Could not connect");
    }
}

void TCPSocket::connect(const std::string& hostname, int port, bool nonblock)
{
    if (m_sock != INVALID_SOCKET) {
        throw std::logic_error("You may only connect an invalid TCPSocket");
    }

    char service[NI_MAXSERV];
    snprintf(service, NI_MAXSERV-1, "%d", port);

    /* Obtain address(es) matching host/port */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo *result, *rp;
    int s = getaddrinfo(hostname.c_str(), service, &hints, &result);
    if (s != 0) {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(s));
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        int sfd = ::socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (nonblock) {
            int flags = fcntl(sfd, F_GETFL);
            if (flags == -1) {
                std::string errstr(strerror(errno));
                freeaddrinfo(result);
                ::close(sfd);
                throw std::runtime_error("TCP: Could not get socket flags: " + errstr);
            }

            if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
                std::string errstr(strerror(errno));
                freeaddrinfo(result);
                ::close(sfd);
                throw std::runtime_error("TCP: Could not set O_NONBLOCK: " + errstr);
            }
        }

        int ret = ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (ret != -1 or (ret == -1 and errno == EINPROGRESS)) {
            m_sock = sfd;
            break;
        }

        ::close(sfd);
    }

    if (m_sock != INVALID_SOCKET) {
#if defined(HAVE_SO_NOSIGPIPE)
        int val = 1;
        if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val))
                == SOCKET_ERROR) {
            throw std::runtime_error("Can't set SO_NOSIGPIPE");
        }
#endif
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == nullptr) {
        throw runtime_error("Could not connect");
    }
}

void TCPSocket::enable_keepalive(int time, int intvl, int probes)
{
    if (m_sock == INVALID_SOCKET) {
        throw std::logic_error("You may not call enable_keepalive on invalid socket");
    }
    int optval = 1;
    auto optlen = sizeof(optval);
    if (setsockopt(m_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set SO_KEEPALIVE: " + errstr);
    }

    optval = time;
    if (setsockopt(m_sock, SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set TCP_KEEPIDLE: " + errstr);
    }

    optval = intvl;
    if (setsockopt(m_sock, SOL_TCP, TCP_KEEPINTVL, &optval, optlen) < 0) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set TCP_KEEPINTVL: " + errstr);
    }

    optval = probes;
    if (setsockopt(m_sock, SOL_TCP, TCP_KEEPCNT, &optval, optlen) < 0) {
        std::string errstr(strerror(errno));
        throw std::runtime_error("TCP: Could not set TCP_KEEPCNT: " + errstr);
    }
}

void TCPSocket::listen(int port, const string& name)
{
    if (m_sock != INVALID_SOCKET) {
        throw std::logic_error("You may only listen with an invalid TCPSocket");
    }

    char service[NI_MAXSERV];
    snprintf(service, NI_MAXSERV-1, "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    struct addrinfo *result, *rp;
    int s = getaddrinfo(name.empty() ? nullptr : name.c_str(), service, &hints, &result);
    if (s != 0) {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(s));
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2).
       If socket(2) (or bind(2)) fails, we (close the socket
       and) try the next address. */
    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        int sfd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        int reuse_setting = 1;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse_setting, sizeof(reuse_setting)) == -1) {
            throw runtime_error("Can't reuse address");
        }

        if (::bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            m_sock = sfd;
            break;
        }

        ::close(sfd);
    }

    freeaddrinfo(result);

    if (m_sock != INVALID_SOCKET) {
#if defined(HAVE_SO_NOSIGPIPE)
        int val = 1;
        if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE,
                    &val, sizeof(val)) < 0) {
            throw std::runtime_error("Can't set SO_NOSIGPIPE");
        }
#endif

        int ret = ::listen(m_sock, 0);
        if (ret == -1) {
            throw std::runtime_error(string("Could not listen: ") + strerror(errno));
        }
    }

    if (rp == nullptr) {
        throw runtime_error("Could not bind");
    }
}

void TCPSocket::close()
{
    ::close(m_sock);
    m_sock = -1;
}

TCPSocket TCPSocket::accept(int timeout_ms)
{
    if (timeout_ms == 0) {
        InetAddress remote_addr;
        socklen_t client_len = sizeof(remote_addr.addr);
        int sockfd = ::accept(m_sock, remote_addr.as_sockaddr(), &client_len);
        TCPSocket s(sockfd, remote_addr);
        return s;
    }
    else {
        struct pollfd fds[1];
        fds[0].fd = m_sock;
        fds[0].events = POLLIN;

        int retval = poll(fds, 1, timeout_ms);

        if (retval == -1) {
            std::string errstr(strerror(errno));
            throw std::runtime_error("TCP Socket accept error: " + errstr);
        }
        else if (retval > 0) {
            InetAddress remote_addr;
            socklen_t client_len = sizeof(remote_addr.addr);
            int sockfd = ::accept(m_sock, remote_addr.as_sockaddr(), &client_len);
            TCPSocket s(sockfd, remote_addr);
            return s;
        }
        else {
            TCPSocket s(-1);
            return s;
        }
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

ssize_t TCPSocket::send(const void* data, size_t size, int timeout_ms)
{
    if (timeout_ms) {
        struct pollfd fds[1];
        fds[0].fd = m_sock;
        fds[0].events = POLLOUT;

        const int retval = poll(fds, 1, timeout_ms);

        if (retval == -1) {
            throw std::runtime_error(string("TCP Socket send error on poll(): ") + strerror(errno));
        }
        else if (retval == 0) {
            // Timed out
            return 0;
        }
    }

    /* On Linux, the MSG_NOSIGNAL flag ensures that the process would not
     * receive a SIGPIPE and die.
     * Other systems have SO_NOSIGPIPE set on the socket for the same effect. */
#if defined(HAVE_MSG_NOSIGNAL)
    const int flags = MSG_NOSIGNAL;
#else
    const int flags = 0;
#endif
    const ssize_t ret = ::send(m_sock, (const char*)data, size, flags);

    if (ret == SOCKET_ERROR) {
            throw std::runtime_error(string("TCP Socket send error: ") + strerror(errno));
    }
    return ret;
}

ssize_t TCPSocket::recv(void *buffer, size_t length, int flags)
{
    ssize_t ret = ::recv(m_sock, buffer, length, flags);
    if (ret == -1) {
        if (errno == EINTR) {
            throw Interrupted();
        }
        else {
            std::string errstr(strerror(errno));
            throw std::runtime_error("TCP receive error: " + errstr);
        }
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
    else if (retval > 0 and (fds[0].revents & POLLIN)) {
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

TCPSocket::TCPSocket(int sockfd) :
    m_sock(sockfd),
    m_remote_address()
{ }

TCPSocket::TCPSocket(int sockfd, InetAddress remote_address) :
    m_sock(sockfd),
    m_remote_address(remote_address)
{ }

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
    TCPSocket newsock;
    m_sock = std::move(newsock);
    m_sock.connect(m_hostname, m_port, true);
}

TCPConnection::TCPConnection(TCPSocket&& sock) :
            queue(),
            m_running(true),
            m_sender_thread(),
            m_sock(move(sock))
{
#if MISSING_OWN_ADDR
    auto own_addr = m_sock.getOwnAddress();
    auto addr = m_sock.getRemoteAddress();
    etiLog.level(debug) << "New TCP Connection on port " <<
        own_addr.getPort() << " from " <<
        addr.getHostAddress() << ":" << addr.getPort();
#endif
    m_sender_thread = std::thread(&TCPConnection::process, this);
}

TCPConnection::~TCPConnection()
{
    m_running = false;
    vector<uint8_t> termination_marker;
    queue.push(termination_marker);
    if (m_sender_thread.joinable()) {
        m_sender_thread.join();
    }
}

void TCPConnection::process()
{
    while (m_running) {
        vector<uint8_t> data;
        queue.wait_and_pop(data);

        if (data.empty()) {
            // empty vector is the termination marker
            m_running = false;
            break;
        }

        try {
            ssize_t remaining = data.size();
            const uint8_t *buf = reinterpret_cast<const uint8_t*>(data.data());
            const int timeout_ms = 10; // Less than one ETI frame

            while (m_running and remaining > 0) {
                const ssize_t sent = m_sock.send(buf, remaining, timeout_ms);
                if (sent < 0 or sent > remaining) {
                    throw std::logic_error("Invalid TCPSocket::send() return value");
                }
                remaining -= sent;
                buf += sent;
            }
        }
        catch (const std::runtime_error& e) {
            m_running = false;
        }
    }

#if MISSING_OWN_ADDR
    auto own_addr = m_sock.getOwnAddress();
    auto addr = m_sock.getRemoteAddress();
    etiLog.level(debug) << "Dropping TCP Connection on port " <<
        own_addr.getPort() << " from " <<
        addr.getHostAddress() << ":" << addr.getPort();
#endif
}


TCPDataDispatcher::TCPDataDispatcher(size_t max_queue_size, size_t buffers_to_preroll) :
    m_max_queue_size(max_queue_size),
    m_buffers_to_preroll(buffers_to_preroll)
{
}

TCPDataDispatcher::~TCPDataDispatcher()
{
    m_running = false;
    m_connections.clear();
    m_listener_socket.close();
    if (m_listener_thread.joinable()) {
        m_listener_thread.join();
    }
}

void TCPDataDispatcher::start(int port, const string& address)
{
    m_listener_socket.listen(port, address);

    m_running = true;
    m_listener_thread = std::thread(&TCPDataDispatcher::process, this);
}

void TCPDataDispatcher::write(const vector<uint8_t>& data)
{
    if (not m_running) {
        throw runtime_error(m_exception_data);
    }

    auto lock = unique_lock<mutex>(m_mutex);

    if (m_buffers_to_preroll > 0) {
        m_preroll_queue.push_back(data);
        if (m_preroll_queue.size() > m_buffers_to_preroll) {
            m_preroll_queue.pop_front();
        }
    }

    for (auto& connection : m_connections) {
        connection.queue.push(data);
    }

    m_connections.remove_if( [&](const TCPConnection& conn){ return conn.queue.size() > m_max_queue_size; });
}

void TCPDataDispatcher::process()
{
    try {
        const int timeout_ms = 1000;

        while (m_running) {
            // Add a new TCPConnection to the list, constructing it from the client socket
            auto sock = m_listener_socket.accept(timeout_ms);
            if (sock.valid()) {
                auto lock = unique_lock<mutex>(m_mutex);
                m_connections.emplace(m_connections.begin(), move(sock));

                if (m_buffers_to_preroll > 0) {
                    for (const auto& buf : m_preroll_queue) {
                        m_connections.front().queue.push(buf);
                    }
                }
            }
        }
    }
    catch (const std::runtime_error& e) {
        m_exception_data = string("TCPDataDispatcher error: ") + e.what();
        m_running = false;
    }
}

TCPReceiveServer::TCPReceiveServer(size_t blocksize) :
    m_blocksize(blocksize)
{
}

void TCPReceiveServer::start(int listen_port, const std::string& address)
{
    m_listener_socket.listen(listen_port, address);

    m_running = true;
    m_listener_thread = std::thread(&TCPReceiveServer::process, this);
}

TCPReceiveServer::~TCPReceiveServer()
{
    m_running = false;
    if (m_listener_thread.joinable()) {
        m_listener_thread.join();
    }
}

shared_ptr<TCPReceiveMessage> TCPReceiveServer::receive()
{
    shared_ptr<TCPReceiveMessage> buffer = make_shared<TCPReceiveMessageEmpty>();
    m_queue.try_pop(buffer);

    // we can ignore try_pop()'s return value, because
    // if it is unsuccessful the buffer is not touched.
    return buffer;
}

void TCPReceiveServer::process()
{
    constexpr int timeout_ms = 1000;
    constexpr int disconnect_timeout_ms = 10000;
    constexpr int max_num_timeouts = disconnect_timeout_ms / timeout_ms;

    while (m_running) {
        auto sock = m_listener_socket.accept(timeout_ms);

        int num_timeouts = 0;

        while (m_running and sock.valid()) {
            try {
                vector<uint8_t> buf(m_blocksize);
                ssize_t r = sock.recv(buf.data(), buf.size(), 0, timeout_ms);
                if (r < 0) {
                    throw logic_error("Invalid recv return value");
                }
                else if (r == 0) {
                    sock.close();
                    m_queue.push(make_shared<TCPReceiveMessageDisconnected>());
                    break;
                }
                else {
                    buf.resize(r);
                    m_queue.push(make_shared<TCPReceiveMessageData>(move(buf)));
                }
            }
            catch (const TCPSocket::Interrupted&) {
                break;
            }
            catch (const TCPSocket::Timeout&) {
                num_timeouts++;
            }
            catch (const runtime_error& e) {
                sock.close();
                // TODO replace fprintf
                fprintf(stderr, "TCP Receiver restarted after error: %s\n", e.what());
                m_queue.push(make_shared<TCPReceiveMessageDisconnected>());
            }

            if (num_timeouts > max_num_timeouts) {
                sock.close();
                m_queue.push(make_shared<TCPReceiveMessageDisconnected>());
            }
        }
    }
}

TCPSendClient::TCPSendClient(const std::string& hostname, int port) :
    m_hostname(hostname),
    m_port(port),
    m_running(true)
{
    m_sender_thread = std::thread(&TCPSendClient::process, this);
}

TCPSendClient::~TCPSendClient()
{
    m_running = false;
    m_queue.trigger_wakeup();
    if (m_sender_thread.joinable()) {
        m_sender_thread.join();
    }
}

void TCPSendClient::sendall(const std::vector<uint8_t>& buffer)
{
    if (not m_running) {
        throw runtime_error(m_exception_data);
    }

    m_queue.push(buffer);

    if (m_queue.size() > MAX_QUEUE_SIZE) {
        vector<uint8_t> discard;
        m_queue.try_pop(discard);
    }
}

void TCPSendClient::process()
{
    try {
        while (m_running) {
            if (m_is_connected) {
                try {
                    vector<uint8_t> incoming;
                    m_queue.wait_and_pop(incoming);
                    if (m_sock.sendall(incoming.data(), incoming.size()) == -1) {
                        m_is_connected = false;
                        m_sock = TCPSocket();
                    }
                }
                catch (const ThreadsafeQueueWakeup&) {
                    break;
                }
            }
            else {
                try {
                    m_sock.connect(m_hostname, m_port);
                    m_is_connected = true;
                }
                catch (const runtime_error& e) {
                    m_is_connected = false;
                    this_thread::sleep_for(chrono::seconds(1));
                }
            }
        }
    }
    catch (const runtime_error& e) {
        m_exception_data = e.what();
        m_running = false;
    }
}

}
