/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
   */
/*
   This file is part of ODR-DabMux.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#pragma once

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "InetAddress.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#define SOCKET           int
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1
#define reuseopt_t       int

#include <stdlib.h>
#include <iostream>
#include <vector>

class UdpPacket;


/**
 *  This class represents a socket for sending and receiving UDP packets.
 *
 *  A UDP socket is the sending or receiving point for a packet delivery service.
 *  Each packet sent or received on a datagram socket is individually
 *  addressed and routed. Multiple packets sent from one machine to another may
 *  be routed differently, and may arrive in any order.
 */
class UdpSocket
{
    public:
        /** Create a new socket that will not be bound to any port. To be used
         * for data output.
         */
        UdpSocket();
        /** Create a new socket.
         *  @param port The port number on which the socket will be bound
         */
        UdpSocket(int port);
        /** Create a new socket.
         *  @param port The port number on which the socket will be bound
         *  @param name The IP address on which the socket will be bound.
         *              It is used to bind the socket on a specific interface if
         *              the computer have many NICs.
         */
        UdpSocket(int port, const std::string& name);
        ~UdpSocket();
        UdpSocket(const UdpSocket& other) = delete;
        const UdpSocket& operator=(const UdpSocket& other) = delete;

        /** reinitialise socket. Close the already open socket, and
         * create a new one
         */
        int reinit(int port, const std::string& name);

        /** Close the socket
         */
        int close(void);

        /** Send an UDP packet.
         *  @param packet The UDP packet to be sent. It includes the data and the
         *                destination address
         *  return 0 if ok, -1 if error
         */
        int send(UdpPacket& packet);

        /** Send an UDP packet
         *
         *  return 0 if ok, -1 if error
         */
        int send(const std::vector<uint8_t>& data, InetAddress destination);

        /** Receive an UDP packet.
         *  @param packet The packet that will receive the data. The address will be set
         *                to the source address.
         *  @return 0 if ok, -1 if error
         */
        int receive(UdpPacket& packet);

        int joinGroup(char* groupname);
        int setMulticastSource(const char* source_addr);
        int setMulticastTTL(int ttl);

        /** Set blocking mode. By default, the socket is blocking.
         *  @return 0  if ok
         *          -1 if error
         */
        int setBlocking(bool block);

    protected:

        /// The address on which the socket is bound.
        InetAddress address;
        /// The low-level socket used by system functions.
        SOCKET listenSocket;
};

/** This class represents a UDP packet.
 *
 *  A UDP packet contains a payload (sequence of bytes) and an address. For
 *  outgoing packets, the address is the destination address. For incoming
 *  packets, the address tells the user from what source the packet arrived from.
 */
class UdpPacket
{
    public:
        /** Construct an empty UDP packet.
         */
        UdpPacket();
        UdpPacket(size_t initSize);

        /** Give the pointer to data.
         *  @return The pointer
         */
        uint8_t* getData(void);

        /** Append some data at the end of data buffer and adjust size.
         *  @param data Pointer to the data to add
         *  @param size Size in bytes of new data
         */
        void addData(const void *data, size_t size);

        size_t getSize(void);

        /** Changes size of the data buffer size. Keeps data intact unless
         *  truncated.
         */
        void setSize(size_t newSize);

        /** Returns the UDP address of the packet.
         */
        InetAddress getAddress(void);

        const std::vector<uint8_t>& getBuffer(void) const {
            return m_buffer;
        }


    private:
        std::vector<uint8_t> m_buffer;
        InetAddress address;
};

