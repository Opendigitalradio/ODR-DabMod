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
#include <list>
#include <string>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

#include "RemoteControl.h"

using boost::asio::ip::tcp;

void
RemoteControllerTelnet::process(long)
{
    welcome_ = "CRC-DABMOD Remote Control CLI\nWrite 'help' for help.\n**********\n";
    prompt_ = "> ";

    std::string in_message;
    size_t length;

    try {
        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port_));

        while (running_) {
            in_message = "";

            tcp::socket socket(io_service);

            acceptor.accept(socket);

            boost::system::error_code ignored_error;

            boost::asio::write(socket, boost::asio::buffer(welcome_),
                    boost::asio::transfer_all(),
                    ignored_error);

            while (running_ && in_message != "quit") {
                boost::asio::write(socket, boost::asio::buffer(prompt_),
                        boost::asio::transfer_all(),
                        ignored_error);

                in_message = "";

                boost::asio::streambuf buffer;
                length = boost::asio::read_until( socket, buffer, "\n", ignored_error);

                std::istream str(&buffer); 
                std::getline(str, in_message);

                if (length == 0) {
                    std::cerr << "Connection terminated" << std::endl;
                    break;
                }

                while (in_message.length() > 0 && 
                        (in_message[in_message.length()-1] == '\r' || in_message[in_message.length()-1] == '\n')) {
                    in_message.erase(in_message.length()-1, 1);
                }

                if (in_message.length() == 0) {
                    continue;
                }

                std::cerr << "Got message '" << in_message << "'" << std::endl;

                dispatch_command(socket, in_message);
            }
            std::cerr << "Closing socket" << std::endl;
            socket.close();
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

void
RemoteControllerTelnet::dispatch_command(tcp::socket& socket, string command)
{
    vector<string> cmd = tokenise_(command);

    if (cmd[0] == "help") {
        reply(socket,
                "The following commands are supported:\n"
                "  list\n"
                "    * Lists the modules that are loaded\n"
                "  list MODULE\n"
                "    * Lists the parameters exported by module MODULE\n"
                "  show MODULE\n"
                "    * Lists all parameters and their values from module MODULE\n"
                "  get MODULE PARAMETER\n"
                "    * Gets the value for the specified PARAMETER from module MODULE\n"
                "  set MODULE PARAMETER VALUE\n"
                "    * Sets the value for the PARAMETER ofr module MODULE\n"
                "  quit\n"
                "    * Terminate this session\n"
                "\n");
    }
    else if (cmd[0] == "list") {
        stringstream ss;

        if (cmd.size() == 1) {
            for (list<RemoteControllable*>::iterator it = cohort_.begin(); it != cohort_.end(); it++) {
                ss << (*it)->get_rc_name() << " ";
            }
        }
        else if (cmd.size() == 2) {
            try {
                stringstream ss;

                list< vector<string> > params = get_parameter_descriptions_(cmd[1]);
                for (list< vector<string> >::iterator it = params.begin(); it != params.end(); it++) {
                    ss << (*it)[0] << " : " << (*it)[1] << endl;
                }
                reply(socket, ss.str());
            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
        }
        else {
            reply(socket, "Too many arguments for command 'list'");
        }

        reply(socket, ss.str());
    }
    else if (cmd[0] == "show") {
        if (cmd.size() == 2) {
            try {
                stringstream ss;
                list< vector<string> > r = get_param_list_values_(cmd[1]);
                for (list< vector<string> >::iterator it = r.begin(); it != r.end(); it++) {
                    ss << (*it)[0] << ": " << (*it)[1] << endl;
                }
                reply(socket, ss.str());

            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
        }
        else
        {
            reply(socket, "Incorrect parameters for command 'show'");
        }
    }
    else if (cmd[0] == "get") {
        if (cmd.size() == 3) {
            try {
                string r = get_param_(cmd[1], cmd[2]);
                reply(socket, r);
            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
        }
        else
        {
            reply(socket, "Incorrect parameters for command 'get'");
        }
    }
    else if (cmd[0] == "set") {
        if (cmd.size() == 4) {
            try {
                set_param_(cmd[1], cmd[2], cmd[3]);
                reply(socket, "ok");
            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
            catch (exception &e) {
                reply(socket, "Error: Invalid parameter value. ");
            }
        }
        else
        {
            reply(socket, "Incorrect parameters for command 'get'");
        }
    }
    else if (cmd[0] == "quit") {
        reply(socket, "Goodbye");
    }
    else {
        reply(socket, "Message not understood");
    }
}

    void
RemoteControllerTelnet::reply(tcp::socket& socket, string message)
{
    boost::system::error_code ignored_error;
    stringstream ss;
    ss << message << "\r\n";
    boost::asio::write(socket, boost::asio::buffer(ss.str()),
            boost::asio::transfer_all(),
            ignored_error);
}

