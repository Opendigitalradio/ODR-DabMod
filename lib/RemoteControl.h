/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   This module adds remote-control capability to some of the dabmux/dabmod modules.
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
#  include "config.h"
#endif

#if defined(HAVE_ZEROMQ)
#  include "zmq.hpp"
#endif

#include <list>
#include <unordered_map>
#include <variant>
#include <map>
#include <memory>
#include <string>
#include <atomic>
#include <iostream>
#include <thread>
#include <stdexcept>

#include "Log.h"
#include "Socket.h"
#include "Json.h"

#define RC_ADD_PARAMETER(p, desc) {   \
  std::vector<std::string> p; \
  p.push_back(#p); \
  p.push_back(desc); \
  m_parameters.push_back(p); \
}

class ParameterError : public std::exception
{
    public:
        ParameterError(std::string message) : m_message(message) {}
        ~ParameterError() throw() {}
        const char* what() const throw() { return m_message.c_str(); }

    private:
        std::string m_message;
};

class RemoteControllable;

/* Remote controllers (that recieve orders from the user)
 * must implement BaseRemoteController
 */
class BaseRemoteController {
    public:
        /* When this returns one, the remote controller cannot be
         * used anymore, and must be restarted
         */
        virtual bool fault_detected() = 0;

        /* In case of a fault, the remote controller can be
         * restarted.
         */
        virtual void restart() = 0;

        virtual ~BaseRemoteController() {}
};

/* Objects that support remote control must implement the following class */
class RemoteControllable {
    public:
        RemoteControllable(const std::string& name) :
            m_rc_name(name) {}

        RemoteControllable(const RemoteControllable& other) = delete;
        RemoteControllable& operator=(const RemoteControllable& other) = delete;

        virtual ~RemoteControllable();

        /* return a short name used to identify the controllable.
         * It might be used in the commands the user has to type, so keep
         * it short
         */
        virtual std::string get_rc_name() const { return m_rc_name; }

        /* Return a list of possible parameters that can be set */
        virtual std::list<std::string> get_supported_parameters() const;

        /* Return a mapping of the descriptions of all parameters */
        virtual std::list< std::vector<std::string> >
            get_parameter_descriptions() const
            {
                return m_parameters;
            }

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter, const std::string& value) = 0;

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const = 0;

        virtual const json::map_t get_all_values() const = 0;

    protected:
        std::string m_rc_name;
        std::list< std::vector<std::string> > m_parameters;
};

/* Holds all our remote controllers and controlled object.
 */
class RemoteControllers {
    public:
        void add_controller(std::shared_ptr<BaseRemoteController> rc);
        void enrol(RemoteControllable *rc);
        void remove_controllable(RemoteControllable *rc);
        void check_faults();
        std::list< std::vector<std::string> > get_param_list_values(const std::string& name);
        std::string get_param(const std::string& name, const std::string& param);
        std::string get_showjson();

        void set_param(
                const std::string& name,
                const std::string& param,
                const std::string& value);

        std::list<RemoteControllable*> controllables;

    private:
        RemoteControllable* get_controllable_(const std::string& name);

        std::list<std::shared_ptr<BaseRemoteController> > m_controllers;
};

/* rcs is a singleton used in all parts of the program to interact with the RC.
 * It is constructed in Globals.cpp */
extern RemoteControllers rcs;

/* Implements a Remote controller based on a simple telnet CLI
 * that listens on localhost
 */
class RemoteControllerTelnet : public BaseRemoteController {
    public:
        RemoteControllerTelnet()
            : m_active(false),
            m_fault(false),
            m_port(0) { }

        RemoteControllerTelnet(int port)
            : m_active(port > 0),
            m_fault(false),
            m_port(port)
        {
            restart();
        }


        RemoteControllerTelnet& operator=(const RemoteControllerTelnet& other) = delete;
        RemoteControllerTelnet(const RemoteControllerTelnet& other) = delete;

        ~RemoteControllerTelnet();

        virtual bool fault_detected() { return m_fault; }

        virtual void restart();

    private:
        void restart_thread(long);

        void process(long);

        void dispatch_command(Socket::TCPSocket& socket, std::string command);
        void reply(Socket::TCPSocket& socket, std::string message);
        void handle_accept(Socket::TCPSocket&& socket);

        std::atomic<bool> m_active;

        /* This is set to true if a fault occurred */
        std::atomic<bool> m_fault;
        std::thread m_restarter_thread;

        std::thread m_child_thread;

        Socket::TCPSocket m_socket;
        int m_port;
};

#if defined(HAVE_ZEROMQ)
/* Implements a Remote controller using ZMQ transportlayer
 * that listens on localhost
 */
class RemoteControllerZmq : public BaseRemoteController {
    public:
        RemoteControllerZmq()
            : m_active(false), m_fault(false),
            m_zmqContext(1),
            m_endpoint("") { }

        RemoteControllerZmq(const std::string& endpoint)
            : m_active(not endpoint.empty()), m_fault(false),
            m_zmqContext(1),
            m_endpoint(endpoint),
            m_child_thread(&RemoteControllerZmq::process, this) { }

        RemoteControllerZmq& operator=(const RemoteControllerZmq& other) = delete;
        RemoteControllerZmq(const RemoteControllerZmq& other) = delete;

        ~RemoteControllerZmq();

        virtual bool fault_detected() { return m_fault; }

        virtual void restart();

    private:
        void restart_thread();

        void recv_all(zmq::socket_t &pSocket, std::vector<std::string> &message);
        void send_ok_reply(zmq::socket_t &pSocket);
        void send_fail_reply(zmq::socket_t &pSocket, const std::string &error);
        void process();

        std::atomic<bool> m_active;

        /* This is set to true if a fault occurred */
        std::atomic<bool> m_fault;
        std::thread m_restarter_thread;

        zmq::context_t m_zmqContext;

        std::string m_endpoint;
        std::thread m_child_thread;
};
#endif

