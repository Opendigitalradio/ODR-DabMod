/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2019
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

UDPPacket::UDPPacket() { }

UDPPacket::UDPPacket(size_t initSize) :
    buffer(initSize)
{ }


UDPSocket::UDPSocket() :
    m_sock(INVALID_SOCKET)
{
    reinit(0, "");
}

UDPSocket::UDPSocket(int port) :
    m_sock(INVALID_SOCKET)
{
    reinit(port, "");
}

UDPSocket::UDPSocket(int port, const std::string& name) :
    m_sock(INVALID_SOCKET)
{
    reinit(port, name);
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
        if (errno == EAGAIN) {
#else
        if (errno == EAGAIN or errno == EWOULDBLOCK) {
#endif
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

UDPReceiver::UDPReceiver() { }

UDPReceiver::~UDPReceiver() {
    m_stop = true;
    m_sock.close();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void UDPReceiver::start(int port, const string& bindto, const string& mcastaddr, size_t max_packets_queued) {
    m_port = port;
    m_bindto = bindto;
    m_mcastaddr = mcastaddr;
    m_max_packets_queued = max_packets_queued;
    m_thread = std::thread(&UDPReceiver::m_run, this);
}

std::vector<uint8_t> UDPReceiver::get_packet_buffer()
{
    if (m_stop) {
        throw runtime_error("UDP Receiver not running");
    }

    UDPPacket p;
    m_packets.wait_and_pop(p);

    return p.buffer;
}

void UDPReceiver::m_run()
{
    // Ensure that stop is set to true in case of exception or return
    struct SetStopOnDestruct {
        SetStopOnDestruct(atomic<bool>& stop) : m_stop(stop) {}
        ~SetStopOnDestruct() { m_stop = true; }
        private: atomic<bool>& m_stop;
    } autoSetStop(m_stop);

    if (IN_MULTICAST(ntohl(inet_addr(m_mcastaddr.c_str())))) {
        m_sock.reinit(m_port, m_mcastaddr);
        m_sock.setMulticastSource(m_bindto.c_str());
        m_sock.joinGroup(m_mcastaddr.c_str(), m_bindto.c_str());
    }
    else {
        m_sock.reinit(m_port, m_bindto);
    }

    while (not m_stop) {
        constexpr size_t packsize = 8192;
        try {
            auto packet = m_sock.receive(packsize);
            if (packet.buffer.size() == packsize) {
                // TODO replace fprintf
                fprintf(stderr, "Warning, possible UDP truncation\n");
            }

            // If this blocks, the UDP socket will lose incoming packets
            m_packets.push_wait_if_full(packet, m_max_packets_queued);
        }
        catch (const std::runtime_error& e) {
            // TODO replace fprintf
            // TODO handle intr
            fprintf(stderr, "Socket error: %s\n", e.what());
            m_stop = true;
        }
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
                throw std::runtime_error("TCP: Could not get socket flags: " + errstr);
            }

            if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1) {
                std::string errstr(strerror(errno));
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
    m_sender_thread.join();
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


TCPDataDispatcher::TCPDataDispatcher(size_t max_queue_size) :
    m_max_queue_size(max_queue_size)
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

    for (auto& connection : m_connections) {
        connection.queue.push(data);
    }

    m_connections.remove_if(
            [&](const TCPConnection& conn){ return conn.queue.size() > m_max_queue_size; });
}

void TCPDataDispatcher::process()
{
    try {
        const int timeout_ms = 1000;

        while (m_running) {
            // Add a new TCPConnection to the list, constructing it from the client socket
            auto sock = m_listener_socket.accept(timeout_ms);
            if (sock.valid()) {
                m_connections.emplace(m_connections.begin(), move(sock));
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

vector<uint8_t> TCPReceiveServer::receive()
{
    vector<uint8_t> buffer;
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
                    break;
                }
                else {
                    buf.resize(r);
                    m_queue.push(move(buf));
                }
            }
            catch (const TCPSocket::Interrupted&) {
                break;
            }
            catch (const TCPSocket::Timeout&) {
                num_timeouts++;
            }

            if (num_timeouts > max_num_timeouts) {
                sock.close();
            }
        }
    }
}

}
