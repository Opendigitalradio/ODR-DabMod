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

#pragma once

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "ThreadsafeQueue.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <list>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#define SOCKET           int
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1


namespace Socket {

struct InetAddress {
    struct sockaddr_storage addr;

    struct sockaddr *as_sockaddr() { return reinterpret_cast<sockaddr*>(&addr); };

    void resolveUdpDestination(const std::string& destination, int port);
};

/** This class represents a UDP packet.
 *
 *  A UDP packet contains a payload (sequence of bytes) and an address. For
 *  outgoing packets, the address is the destination address. For incoming
 *  packets, the address tells the user from what source the packet arrived from.
 */
class UDPPacket
{
    public:
        UDPPacket();
        UDPPacket(size_t initSize);

        std::vector<uint8_t> buffer;
        InetAddress address;
};

/**
 *  This class represents a socket for sending and receiving UDP packets.
 *
 *  A UDP socket is the sending or receiving point for a packet delivery service.
 *  Each packet sent or received on a datagram socket is individually
 *  addressed and routed. Multiple packets sent from one machine to another may
 *  be routed differently, and may arrive in any order.
 */
class UDPSocket
{
    public:
        /** Create a new socket that will not be bound to any port. To be used
         * for data output.
         */
        UDPSocket();
        /** Create a new socket.
         *  @param port The port number on which the socket will be bound
         */
        UDPSocket(int port);
        /** Create a new socket.
         *  @param port The port number on which the socket will be bound
         *  @param name The IP address on which the socket will be bound.
         *              It is used to bind the socket on a specific interface if
         *              the computer have many NICs.
         */
        UDPSocket(int port, const std::string& name);
        ~UDPSocket();
        UDPSocket(const UDPSocket& other) = delete;
        const UDPSocket& operator=(const UDPSocket& other) = delete;

        /** Close the already open socket, and create a new one. Throws a runtime_error on error.  */
        void reinit(int port);
        void reinit(int port, const std::string& name);

        void close(void);
        void send(UDPPacket& packet);
        void send(const std::vector<uint8_t>& data, InetAddress destination);
        UDPPacket receive(size_t max_size);
        void joinGroup(const char* groupname, const char* if_addr = nullptr);
        void setMulticastSource(const char* source_addr);
        void setMulticastTTL(int ttl);

        /** Set blocking mode. By default, the socket is blocking.
         * throws a runtime_error on error.
         */
        void setBlocking(bool block);

    protected:
        SOCKET m_sock;
};

/* Threaded UDP receiver */
class UDPReceiver {
    public:
        UDPReceiver();
        ~UDPReceiver();
        UDPReceiver(const UDPReceiver&) = delete;
        UDPReceiver operator=(const UDPReceiver&) = delete;

        // Start the receiver in a separate thread
        void start(int port, const std::string& bindto, const std::string& mcastaddr, size_t max_packets_queued);

        // Get the data contained in a UDP packet, blocks if none available
        // In case of error, throws a runtime_error
        std::vector<uint8_t> get_packet_buffer(void);

    private:
        void m_run(void);

        int m_port = 0;
        std::string m_bindto;
        std::string m_mcastaddr;
        size_t m_max_packets_queued = 1;
        std::thread m_thread;
        std::atomic<bool> m_stop = ATOMIC_VAR_INIT(false);
        ThreadsafeQueue<UDPPacket> m_packets;
        UDPSocket m_sock;
};

class TCPSocket {
    public:
        TCPSocket();
        ~TCPSocket();
        TCPSocket(const TCPSocket& other) = delete;
        TCPSocket& operator=(const TCPSocket& other) = delete;
        TCPSocket(TCPSocket&& other);
        TCPSocket& operator=(TCPSocket&& other);

        bool valid(void) const;
        void connect(const std::string& hostname, int port, bool nonblock = false);
        void listen(int port, const std::string& name);
        void close(void);

        /* throws a runtime_error on failure, an invalid socket on timeout */
        TCPSocket accept(int timeout_ms);

        /* returns -1 on error, doesn't work on nonblocking sockets */
        ssize_t sendall(const void *buffer, size_t buflen);

        /** Send data over the TCP connection.
         *  @param data The buffer that will be sent.
         *  @param size Number of bytes to send.
         *  @param timeout_ms number of milliseconds before timeout, or 0 for infinite timeout
         *  return number of bytes sent, 0 on timeout, or throws runtime_error.
         */
        ssize_t send(const void* data, size_t size, int timeout_ms=0);

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
        explicit TCPSocket(int sockfd, InetAddress remote_address);
        SOCKET m_sock = -1;

        InetAddress m_remote_address;

        friend class TCPClient;
};

/* Implements a TCP receiver that auto-reconnects on errors */
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

/* Helper class for TCPDataDispatcher, contains a queue of pending data and
 * a sender thread. */
class TCPConnection
{
    public:
        TCPConnection(TCPSocket&& sock);
        TCPConnection(const TCPConnection&) = delete;
        TCPConnection& operator=(const TCPConnection&) = delete;
        ~TCPConnection();

        ThreadsafeQueue<std::vector<uint8_t> > queue;

    private:
        std::atomic<bool> m_running;
        std::thread m_sender_thread;
        TCPSocket m_sock;

        void process(void);
};

/* Send a TCP stream to several destinations, and automatically disconnect destinations
 * whose buffer overflows.
 */
class TCPDataDispatcher
{
    public:
        TCPDataDispatcher(size_t max_queue_size);
        ~TCPDataDispatcher();
        TCPDataDispatcher(const TCPDataDispatcher&) = delete;
        TCPDataDispatcher& operator=(const TCPDataDispatcher&) = delete;

        void start(int port, const std::string& address);
        void write(const std::vector<uint8_t>& data);

    private:
        void process();

        size_t m_max_queue_size;

        std::atomic<bool> m_running;
        std::string m_exception_data;
        std::thread m_listener_thread;
        TCPSocket m_listener_socket;
        std::list<TCPConnection> m_connections;
};

/* A TCP Server to receive data, which abstracts the handling of connects and disconnects.
 */
class TCPReceiveServer {
    public:
        TCPReceiveServer(size_t blocksize);
        ~TCPReceiveServer();
        TCPReceiveServer(const TCPReceiveServer&) = delete;
        TCPReceiveServer& operator=(const TCPReceiveServer&) = delete;

        void start(int listen_port, const std::string& address);

        // Return a vector that contains up to blocksize bytes of data, or
        // and empty vector if no data is available.
        std::vector<uint8_t> receive();

    private:
        void process();

        size_t m_blocksize = 0;
        ThreadsafeQueue<std::vector<uint8_t> > m_queue;
        std::atomic<bool> m_running;
        std::string m_exception_data;
        std::thread m_listener_thread;
        TCPSocket m_listener_socket;
};

}
