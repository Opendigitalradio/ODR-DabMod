/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Written by Matthias P. Braendli, matthias.braendli@mpb.li, 2012

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

#include <list>
#include <map>
#include <string>
#include <iostream>
#include <string>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/thread.hpp>
#include <stdexcept>


#define RC_ADD_PARAMETER(p, desc) {   \
  vector<string> p; \
  p.push_back(#p); \
  p.push_back(desc); \
  m_parameters.push_back(p); \
}


using namespace std;
using boost::asio::ip::tcp;

class ParameterError : public std::exception
{
    public:
        ParameterError(string message) : m_message(message) {}
        ~ParameterError() throw() {};
        const char* what() const throw() { return m_message.c_str(); }

    private:
        string m_message;
};

class RemoteControllable;

/* Remote controllers (that recieve orders from the user)
 * must implement BaseRemoteController
 */
class BaseRemoteController {
    public:
        /* Add a new controllable under this controller's command */
        virtual void enrol(RemoteControllable* controllable) = 0;

        /* When this returns one, the remote controller cannot be
         * used anymore, and must be restarted by dabmux
         */
        virtual bool fault_detected() = 0;

        /* In case of a fault, the remote controller can be
         * restarted.
         */
        virtual void restart() = 0;
};

/* Objects that support remote control must implement the following class */
class RemoteControllable {
    public:

        RemoteControllable(string name) : m_name(name) {}

        /* return a short name used to identify the controllable.
         * It might be used in the commands the user has to type, so keep
         * it short
         */
        virtual std::string get_rc_name() const { return m_name; }

        /* Tell the controllable to enrol at the given controller */
        virtual void enrol_at(BaseRemoteController& controller) {
            controller.enrol(this);
        }

        /* Return a list of possible parameters that can be set */
        virtual list<string> get_supported_parameters() const {
            list<string> parameterlist;
            for (list< vector<string> >::const_iterator it = m_parameters.begin();
                    it != m_parameters.end(); ++it) {
                parameterlist.push_back((*it)[0]);
            }
            return parameterlist;
        }

        /* Return a mapping of the descriptions of all parameters */
        virtual std::list< std::vector<std::string> >
            get_parameter_descriptions() const {
            return m_parameters;
        }

        /* Base function to set parameters. */
        virtual void set_parameter(const string& parameter,
                const string& value) = 0;

        /* Getting a parameter always returns a string. */
        virtual const string get_parameter(const string& parameter) const = 0;

    protected:
        std::string m_name;
        std::list< std::vector<std::string> > m_parameters;
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

        virtual bool fault_detected() { return m_fault; };

        virtual void restart();

    private:
        void restart_thread(long);

        void process(long);

        void dispatch_command(tcp::socket& socket, string command);

        void reply(tcp::socket& socket, string message);

        RemoteControllerTelnet& operator=(const RemoteControllerTelnet& other);
        RemoteControllerTelnet(const RemoteControllerTelnet& other);

        vector<string> tokenise_(string message) {
            vector<string> all_tokens;

            boost::char_separator<char> sep(" ");
            boost::tokenizer< boost::char_separator<char> > tokens(message, sep);
            BOOST_FOREACH (const string& t, tokens) {
                all_tokens.push_back(t);
            }
            return all_tokens;
        }

        RemoteControllable* get_controllable_(string name) {
            for (list<RemoteControllable*>::iterator it = m_cohort.begin();
                    it != m_cohort.end(); ++it) {
                if ((*it)->get_rc_name() == name)
                {
                    return *it;
                }
            }
            throw ParameterError("Module name unknown");
        }

        list< vector<string> > get_parameter_descriptions_(string name) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_parameter_descriptions();
        }

        list<string> get_param_list_(string name) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_supported_parameters();
        }

        list< vector<string> > get_param_list_values_(string name) {
            RemoteControllable* controllable = get_controllable_(name);

            list< vector<string> > allparams;
            list<string> params = controllable->get_supported_parameters();
            for (list<string>::iterator it = params.begin();
                    it != params.end(); ++it) {
                vector<string> item;
                item.push_back(*it);
                item.push_back(controllable->get_parameter(*it));

                allparams.push_back(item);
            }
            return allparams;
        }

        string get_param_(string name, string param) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->get_parameter(param);
        }

        void set_param_(string name, string param, string value) {
            RemoteControllable* controllable = get_controllable_(name);
            return controllable->set_parameter(param, value);
        }

        bool m_running;

        /* This is set to true if a fault occurred */
        bool m_fault;
        boost::thread m_restarter_thread;

        boost::thread m_child_thread;

        /* This controller commands the controllables in the cohort */
        list<RemoteControllable*> m_cohort;

        std::string m_welcome;
        std::string m_prompt;

        int m_port;
};


/* The Dummy remote controller does nothing, and never fails
 */
class RemoteControllerDummy : public BaseRemoteController {
    public:
        void enrol(RemoteControllable* controllable) {};

        bool fault_detected() { return false; };

        virtual void restart() {};
};

#endif

