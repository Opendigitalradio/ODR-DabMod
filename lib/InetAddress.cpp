/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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

#include "InetAddress.h"
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef TRACE_ON
# ifndef TRACE_CLASS
#  define TRACE_CLASS(clas, func) cout <<"-" <<(clas) <<"\t(" <<this <<")::" <<(func) <<endl
#  define TRACE_STATIC(clas, func) cout <<"-" <<(clas) <<"\t(static)::" <<(func) <<endl
# endif
#else
# ifndef TRACE_CLASS
#  define TRACE_CLASS(clas, func)
#  define TRACE_STATIC(clas, func)
# endif
#endif


int inetErrNo = 0;
const char *inetErrMsg = nullptr;
const char *inetErrDesc = nullptr;


/**
 *  Constructs an IP address.
 *  @param port The port of this address
 *  @param name The name of this address
 */
InetAddress::InetAddress(int port, const char* name) {
    TRACE_CLASS("InetAddress", "InetAddress(int, char)");
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (name)
        setAddress(name);
}


/**
 *  Constructs a copy of inet
 *  @param inet The address to be copied
 */
InetAddress::InetAddress(const InetAddress &inet) {
    TRACE_CLASS("InetAddress", "InetAddress(InetAddress)");
    memcpy(&addr, &inet.addr, sizeof(addr));
}


/// Destructor
InetAddress::~InetAddress() {
    TRACE_CLASS("InetAddress" ,"~InetAddress()");
}


/// Returns the raw IP address of this InetAddress object.
sockaddr *InetAddress::getAddress() {
    TRACE_CLASS("InetAddress", "getAddress()");
    return (sockaddr *)&addr;
}


/// Return the port of this address.
int InetAddress::getPort()
{
    TRACE_CLASS("InetAddress", "getPort()");
    return ntohs(addr.sin_port);
}


/**
 *  Returns the IP address string "%d.%d.%d.%d".
 *  @return IP address
 */
const char *InetAddress::getHostAddress() {
    TRACE_CLASS("InetAddress", "getHostAddress()");
    return inet_ntoa(addr.sin_addr);
}


/// Returns true if this address is multicast
bool InetAddress::isMulticastAddress() {
    TRACE_CLASS("InetAddress", "isMulticastAddress()");
    return IN_MULTICAST(ntohl(addr.sin_addr.s_addr));		// a modifier
}


/**
 *  Set the port number
 *  @param port The new port number
 */
void InetAddress::setPort(int port)
{
    TRACE_CLASS("InetAddress", "setPort(int)");
    addr.sin_port = htons(port);
}


/**
 *  Set the address
 *  @param name The new address name
 *  @return 0  if ok
 *          -1 if error
 */
int InetAddress::setAddress(const std::string& name)
{
    TRACE_CLASS("InetAddress", "setAddress(string)");
    if (!name.empty()) {
        if (atoi(name.c_str())) {   // If it start with a number
            if ((addr.sin_addr.s_addr = inet_addr(name.c_str())) == INADDR_NONE) {
                addr.sin_addr.s_addr = htons(INADDR_ANY);
                inetErrNo = 0;
                inetErrMsg = "Invalid address";
                inetErrDesc = name.c_str();
                return -1;
            }
        }
        else {            // Assume it's a real name
            hostent *host = gethostbyname(name.c_str());
            if (host) {
                addr.sin_addr = *(in_addr *)(host->h_addr);
            } else {
                addr.sin_addr.s_addr = htons(INADDR_ANY);
                inetErrNo = 0;
                inetErrMsg = "Could not find address";
                inetErrDesc = name.c_str();
                return -1;
            }
        }
    }
    else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    return 0;
}


void setInetError(const char* description)
{
    inetErrNo = 0;
    inetErrNo = errno;
    inetErrMsg = strerror(inetErrNo);
    inetErrDesc = description;
}

