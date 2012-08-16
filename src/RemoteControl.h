/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Written by Matthias P. Braendli, matthias.braendli@mpb.li, 2012
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
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

using namespace std;
using boost::asio::ip::tcp;

class ParameterError : public std::exception
{
    public:
        ParameterError(string message) : message_(message) {}
        ~ParameterError() throw() {};
        const char* what() { return message_.c_str(); }

    private:
        string message_;
};

class RemoteControllable;

/* Remote controllers (that recieve orders from the user) must implement BaseRemoteController */
class BaseRemoteController {
    public:
        /* Add a new controllable under this controller's command */
        virtual void enrol(RemoteControllable* controllable) = 0;
};

/* Objects that support remote control must implement the following class */
class RemoteControllable {
    public:
        /* return a short name used to identify the controllable.
         * It might be used in the commands the user has to type, so keep
         * it short
         */
        virtual std::string get_rc_name() = 0;
        
        /* Tell the controllable to enrol at the given controller */
        virtual void enrol_at(BaseRemoteController& controller) {
            controller.enrol(this);
        }

        /* Return a list of possible parameters that can be set */
        virtual list<string> get_supported_parameters() = 0;

        /* Return a mapping of the descriptions of all parameters */
        virtual std::list< std::vector<std::string> > get_parameter_descriptions() = 0;

        /* Base function to set parameters. */
        virtual void set_parameter(string parameter, string value) = 0;

        /* Convenience functions for other common types */
        virtual void set_parameter(string parameter, double value) = 0;
        virtual void set_parameter(string parameter, long value) = 0;

        /* Getting a parameter always returns a string. */
        virtual string get_parameter(string parameter) = 0;
};

/* Implements a Remote controller based on a simple telnet CLI
 * that listens on localhost
 */
class RemoteControllerTelnet : public BaseRemoteController {
    public:
        RemoteControllerTelnet(int port) {
            port_ = port;
            running_ = false;
        };

        void start() {
            running_ = true;
            child_thread_ = boost::thread(&RemoteControllerTelnet::process, this, 0);
        }

        void stop() {
            running_ = false;
            child_thread_.interrupt();
            child_thread_.join();
        }

        void process(long);

        void dispatch_command(tcp::socket& socket, string command);

        void reply(tcp::socket& socket, string message);

        void enrol(RemoteControllable* controllable) {
            cohort_.push_back(controllable);
        }


    private:
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
            for (list<RemoteControllable*>::iterator it = cohort_.begin(); it != cohort_.end(); it++) {
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
            for (list<string>::iterator it = params.begin(); it != params.end(); it++) {
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

        bool running_;
        boost::thread child_thread_;

        /* This controller commands the controllables in the cohort */
        list<RemoteControllable*> cohort_;

        std::string welcome_;
        std::string prompt_;

        int port_;
};

#endif
