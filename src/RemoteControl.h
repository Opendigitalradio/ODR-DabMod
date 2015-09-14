/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

   This module adds remote-control capability to some of the dabmod modules.
   see testremotecontrol/test.cpp for an example of how to use this.
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

#ifndef _REMOTECONTROL_H
#define _REMOTECONTROL_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if defined(HAVE_ZEROMQ)
#include "zmq.hpp"
#endif

#include <list>
#include <map>
#include <string>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>
#include <stdexcept>

#include "Log.h"

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
        /* Add a new controllable under this controller's command */
        virtual void enrol(RemoteControllable* controllable) = 0;

        /* Remove a controllable under this controller's command */
        virtual void disengage(RemoteControllable* controllable) = 0;

        /* When this returns one, the remote controller cannot be
         * used anymore, and must be restarted by dabmux
         */
        virtual bool fault_detected() = 0;

        /* In case of a fault, the remote controller can be
         * restarted.
         */
        virtual void restart() = 0;

        virtual ~BaseRemoteController() {}
};

/* Holds all our remote controllers, i.e. we may have more than
 * one type of controller running.
 */
class RemoteControllers {
    public:
        void add_controller(BaseRemoteController *rc) {
            m_controllers.push_back(rc);
        }

        void add_controllable(RemoteControllable *rc) {
            for (auto &controller : m_controllers) {
                controller->enrol(rc);
            }
        }

        void remove_controllable(RemoteControllable *rc) {
            for (auto &controller : m_controllers) {
                controller->disengage(rc);
            }
        }

        void check_faults() {
            for (auto &controller : m_controllers) {
                if (controller->fault_detected())
                {
                    etiLog.level(warn) <<
                            "Detected Remote Control fault, restarting it";
                    controller->restart();
                }
            }
        }
        size_t get_no_controllers() { return m_controllers.size(); }

    private:
        std::list<BaseRemoteController*> m_controllers;
};

/* Objects that support remote control must implement the following class */
class RemoteControllable {
    public:

        RemoteControllable(std::string name) :
            m_rcs_enrolled_at(nullptr),
            m_name(name) {}

        virtual ~RemoteControllable() {
            if (m_rcs_enrolled_at) {
                m_rcs_enrolled_at->remove_controllable(this);
            }
        }

        /* return a short name used to identify the controllable.
         * It might be used in the commands the user has to type, so keep
         * it short
         */
        virtual std::string get_rc_name() const { return m_name; }

        /* Tell the controllable to enrol at the given controller */
        virtual void enrol_at(RemoteControllers& controllers) {
            if (m_rcs_enrolled_at) {
                throw std::runtime_error("This controllable is already enrolled");
            }

            m_rcs_enrolled_at = &controllers;
            controllers.add_controllable(this);
        }

        /* Return a list of possible parameters that can be set */
        virtual std::list<std::string> get_supported_parameters() const {
            std::list<std::string> parameterlist;
            for (const auto& param : m_parameters) {
                parameterlist.push_back(param[0]);
            }
            return parameterlist;
        }

        /* Return a mapping of the descriptions of all parameters */
        virtual std::list< std::vector<std::string> >
            get_parameter_descriptions() const {
            return m_parameters;
        }

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value) = 0;

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const = 0;

    protected:
        RemoteControllers* m_rcs_enrolled_at;
        std::string m_name;
        std::list< std::vector<std::string> > m_parameters;

        RemoteControllable(const RemoteControllable& other) = delete;
        RemoteControllable& operator=(const RemoteControllable& other) = delete;
};

/* Implements a Remote controller based on a simple telnet CLI
 * that listens on localhost
 */
class RemoteControllerTelnet : public BaseRemoteController {
    public:
        RemoteControllerTelnet()
            : m_running(false), m_fault(false),
            m_port(0) { }

        RemoteControllerTelnet(int port)
            : m_running(true), m_fault(false),
            m_child_thread(&RemoteControllerTelnet::process, this, 0),
            m_port(port)
        { }

        ~RemoteControllerTelnet() {
            m_running = false;
            m_fault = false;
            if (m_port) {
                m_child_thread.interrupt();
                m_child_thread.join();
            }
        }

        void enrol(RemoteControllable* controllable) {
            m_cohort.push_back(controllable);
        }

        void disengage(RemoteControllable* controllable) {
            m_cohort.remove(controllable);
        }

        virtual bool fault_detected() { return m_fault; }

        virtual void restart();

    private:
        void restart_thread(long);

        void process(long);

        void dispatch_command(boost::asio::ip::tcp::socket& socket,
                std::string command);

        void reply(boost::asio::ip::tcp::socket& socket, std::string message);

        RemoteControllerTelnet& operator=(const RemoteControllerTelnet& other);
        RemoteControllerTelnet(const RemoteControllerTelnet& other);

        std::vector<std::string> tokenise_(std::string message) {
            std::vector<std::string> all_tokens;

            boost::char_separator<char> sep(" ");
            boost::tokenizer< boost::char_separator<char> > tokens(message, sep);
            BOOST_FOREACH (const std::string& t, tokens) {
                all_tokens.push_back(t);
            }
            return all_tokens;
        }

        RemoteControllable* get_controllable_(std::string name) {
            for (std::list<RemoteControllable*>::iterator it = m_cohort.begin();
                    it != m_cohort.end(); ++it) {
                if ((*it)->get_rc_name() == name)
                {
                    return *it;
                }
            }
            throw ParameterError("Module name unknown");
        }

        std::list< std::vector<std::string> >
            get_parameter_descriptions_(std::string name) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_parameter_descriptions();
        }

        std::list<std::string> get_param_list_(std::string name) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_supported_parameters();
        }

        std::list< std::vector<std::string> >
            get_param_list_values_(std::string name) {
            RemoteControllable* controllable = get_controllable_(name);

            std::list< std::vector<std::string> > allparams;
            for (auto &param : controllable->get_supported_parameters()) {
                std::vector<std::string> item;
                item.push_back(param);
                item.push_back(controllable->get_parameter(param));

                allparams.push_back(item);
            }
            return allparams;
        }

        std::string get_param_(std::string name, std::string param) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_parameter(param);
        }

        void set_param_(std::string name, std::string param, std::string value) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->set_parameter(param, value);
        }

        bool m_running;

        /* This is set to true if a fault occurred */
        bool m_fault;
        boost::thread m_restarter_thread;

        boost::thread m_child_thread;

        /* This controller commands the controllables in the cohort */
        std::list<RemoteControllable*> m_cohort;

        int m_port;
};

#if defined(HAVE_ZEROMQ)
/* Implements a Remote controller using zmq transportlayer
 * that listens on localhost
 */
class RemoteControllerZmq : public BaseRemoteController {
    public:
        RemoteControllerZmq()
            : m_running(false), m_fault(false),
            m_zmqContext(1),
            m_endpoint("") { }

        RemoteControllerZmq(std::string endpoint)
            : m_running(true), m_fault(false),
            m_zmqContext(1),
            m_endpoint(endpoint),
            m_child_thread(&RemoteControllerZmq::process, this) { }

        ~RemoteControllerZmq() {
            m_running = false;
            m_fault = false;
            if (!m_endpoint.empty()) {
                m_child_thread.interrupt();
                m_child_thread.join();
            }
        }

        void enrol(RemoteControllable* controllable) {
            m_cohort.push_back(controllable);
        }

        void disengage(RemoteControllable* controllable) {
            m_cohort.remove(controllable);
        }

        virtual bool fault_detected() { return m_fault; }

        virtual void restart();

    private:
        void restart_thread();

        void recv_all(zmq::socket_t &pSocket, std::vector<std::string> &message);
        void send_ok_reply(zmq::socket_t &pSocket);
        void send_fail_reply(zmq::socket_t &pSocket, const std::string &error);
        void process();


        RemoteControllerZmq& operator=(const RemoteControllerZmq& other);
        RemoteControllerZmq(const RemoteControllerZmq& other);

        RemoteControllable* get_controllable_(std::string name) {
            for (std::list<RemoteControllable*>::iterator it = m_cohort.begin();
                    it != m_cohort.end(); ++it) {
                if ((*it)->get_rc_name() == name)
                {
                    return *it;
                }
            }
            throw ParameterError("Module name unknown");
        }

        std::string get_param_(std::string name, std::string param) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_parameter(param);
        }

        void set_param_(std::string name, std::string param, std::string value) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->set_parameter(param, value);
        }

        std::list< std::vector<std::string> >
            get_param_list_values_(std::string name) {
            RemoteControllable* controllable = get_controllable_(name);

            std::list< std::vector<std::string> > allparams;

            for (auto &param : controllable->get_supported_parameters()) {
                std::vector<std::string> item;
                item.push_back(param);
                item.push_back(controllable->get_parameter(param));

                allparams.push_back(item);
            }

            return allparams;
        }


        bool m_running;

        /* This is set to true if a fault occurred */
        bool m_fault;
        boost::thread m_restarter_thread;

        zmq::context_t m_zmqContext;

        /* This controller commands the controllables in the cohort */
        std::list<RemoteControllable*> m_cohort;

        std::string m_endpoint;
        boost::thread m_child_thread;
};
#endif

/* The Dummy remote controller does nothing, and never fails
 */
class RemoteControllerDummy : public BaseRemoteController {
    public:
        void enrol(RemoteControllable*) {}
        void disengage(RemoteControllable*) {}

        bool fault_detected() { return false; }

        virtual void restart() {}
};

#endif

