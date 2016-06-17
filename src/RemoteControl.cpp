/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li
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
#include <list>
#include <string>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "RemoteControl.h"
#include "Utils.h"

using boost::asio::ip::tcp;
using namespace std;


void RemoteControllerTelnet::restart()
{
    m_restarter_thread = boost::thread(&RemoteControllerTelnet::restart_thread,
            this, 0);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void RemoteControllerTelnet::restart_thread(long)
{
    m_running = false;

    if (m_port) {
        m_child_thread.interrupt();
        m_child_thread.join();
    }

    m_child_thread = boost::thread(&RemoteControllerTelnet::process, this, 0);
}

void RemoteControllerTelnet::process(long)
{
    set_thread_name("telnet_rc");
    std::string m_welcome = "ODR-DabMod Remote Control CLI\n"
                            "Write 'help' for help.\n"
                            "**********\n";
    std::string m_prompt = "> ";

    std::string in_message;
    size_t length;

    try {
        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(
                    boost::asio::ip::address::from_string("127.0.0.1"), m_port) );

        while (m_running) {
            in_message = "";

            tcp::socket socket(io_service);

            acceptor.accept(socket);

            boost::system::error_code ignored_error;

            boost::asio::write(socket, boost::asio::buffer(m_welcome),
                    boost::asio::transfer_all(),
                    ignored_error);

            while (m_running && in_message != "quit") {
                boost::asio::write(socket, boost::asio::buffer(m_prompt),
                        boost::asio::transfer_all(),
                        ignored_error);

                in_message = "";

                boost::asio::streambuf buffer;
                length = boost::asio::read_until( socket, buffer, "\n", ignored_error);

                std::istream str(&buffer); 
                std::getline(str, in_message);

                if (length == 0) {
                    std::cerr << "RC: Connection terminated" << std::endl;
                    break;
                }

                while (in_message.length() > 0 && 
                        (in_message[in_message.length()-1] == '\r' ||
                         in_message[in_message.length()-1] == '\n')) {
                    in_message.erase(in_message.length()-1, 1);
                }

                if (in_message.length() == 0) {
                    continue;
                }

                std::cerr << "RC: Got message '" << in_message << "'" << std::endl;

                dispatch_command(socket, in_message);
            }
            std::cerr << "RC: Closing socket" << std::endl;
            socket.close();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Remote control caught exception: " << e.what() << std::endl;
        m_fault = true;
    }
}

void RemoteControllerTelnet::dispatch_command(tcp::socket& socket, string command)
{
    vector<string> cmd = tokenise_(command);

    if (cmd[0] == "help") {
        reply(socket,
                "The following commands are supported:\n"
                "  list\n"
                "    * Lists the modules that are loaded and their parameters\n"
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
            for (auto &controllable : m_cohort) {
                ss << controllable->get_rc_name() << endl;

                list< vector<string> > params = controllable->get_parameter_descriptions();
                for (auto &param : params) {
                    ss << "\t" << param[0] << " : " << param[1] << endl;
                }
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
                for (auto &param_val : r) {
                    ss << param_val[0] << ": " << param_val[1] << endl;
                }
                reply(socket, ss.str());

            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
        }
        else {
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
        else {
            reply(socket, "Incorrect parameters for command 'get'");
        }
    }
    else if (cmd[0] == "set") {
        if (cmd.size() >= 4) {
            try {
                stringstream new_param_value;
                for (size_t i = 3; i < cmd.size(); i++) {
                    new_param_value << cmd[i];

                    if (i+1 < cmd.size()) {
                        new_param_value << " ";
                    }
                }

                set_param_(cmd[1], cmd[2], new_param_value.str());
                reply(socket, "ok");
            }
            catch (ParameterError &e) {
                reply(socket, e.what());
            }
            catch (exception &e) {
                reply(socket, "Error: Invalid parameter value. ");
            }
        }
        else {
            reply(socket, "Incorrect parameters for command 'set'");
        }
    }
    else if (cmd[0] == "quit") {
        reply(socket, "Goodbye");
    }
    else {
        reply(socket, "Message not understood");
    }
}

void RemoteControllerTelnet::reply(tcp::socket& socket, string message)
{
    boost::system::error_code ignored_error;
    stringstream ss;
    ss << message << "\r\n";
    boost::asio::write(socket, boost::asio::buffer(ss.str()),
            boost::asio::transfer_all(),
            ignored_error);
}


#if defined(HAVE_ZEROMQ)

void RemoteControllerZmq::restart()
{
    m_restarter_thread = boost::thread(&RemoteControllerZmq::restart_thread, this);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void RemoteControllerZmq::restart_thread()
{
    m_running = false;

    if (!m_endpoint.empty()) {
        m_child_thread.interrupt();
        m_child_thread.join();
    }

    m_child_thread = boost::thread(&RemoteControllerZmq::process, this);
}

void RemoteControllerZmq::recv_all(zmq::socket_t& pSocket, std::vector<std::string> &message)
{
    bool more = true;
    do {
        zmq::message_t msg;
        pSocket.recv(&msg);
        std::string incoming((char*)msg.data(), msg.size());
        message.push_back(incoming);
        more = msg.more();
    } while (more);
}

void RemoteControllerZmq::send_ok_reply(zmq::socket_t &pSocket)
{
    zmq::message_t msg(2);
    char repCode[2] = {'o', 'k'};
    memcpy ((void*) msg.data(), repCode, 2);
    pSocket.send(msg, 0);
}

void RemoteControllerZmq::send_fail_reply(zmq::socket_t &pSocket, const std::string &error)
{
    zmq::message_t msg1(4);
    char repCode[4] = {'f', 'a', 'i', 'l'};
    memcpy ((void*) msg1.data(), repCode, 4);
    pSocket.send(msg1, ZMQ_SNDMORE);

    zmq::message_t msg2(error.length());
    memcpy ((void*) msg2.data(), error.c_str(), error.length());
    pSocket.send(msg2, 0);
}

void RemoteControllerZmq::process()
{
    set_thread_name("zmq_rc");
    // create zmq reply socket for receiving ctrl parameters
    etiLog.level(info) << "Starting zmq remote control thread";
    try {
        zmq::socket_t repSocket(m_zmqContext, ZMQ_REP);

        // connect the socket
        int hwm = 100;
        int linger = 0;
        repSocket.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
        repSocket.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
        repSocket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        repSocket.bind(m_endpoint.c_str());

        // create pollitem that polls the  ZMQ sockets
        zmq::pollitem_t pollItems[] = { {repSocket, 0, ZMQ_POLLIN, 0} };
        for (;;) {
            zmq::poll(pollItems, 1, 100);
            std::vector<std::string> msg;

            if (pollItems[0].revents & ZMQ_POLLIN) {
                recv_all(repSocket, msg);

                std::string command((char*)msg[0].data(), msg[0].size());

                if (msg.size() == 1 && command == "ping") {
                    send_ok_reply(repSocket);
                }
                else if (msg.size() == 1 && command == "list") {
                    size_t cohort_size = m_cohort.size();
                    for (auto &controllable : m_cohort) {
                        std::stringstream ss;
                        ss << controllable->get_rc_name();

                        std::string msg_s = ss.str();

                        zmq::message_t msg(ss.str().size());
                        memcpy ((void*) msg.data(), msg_s.data(), msg_s.size());

                        int flag = (--cohort_size > 0) ? ZMQ_SNDMORE : 0;
                        repSocket.send(msg, flag);
                    }
                }
                else if (msg.size() == 2 && command == "show") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    try {
                        list< vector<string> > r = get_param_list_values_(module);
                        size_t r_size = r.size();
                        for (auto &param_val : r) {
                            std::stringstream ss;
                            ss << param_val[0] << ": " << param_val[1] << endl;
                            zmq::message_t msg(ss.str().size());
                            memcpy(msg.data(), ss.str().data(), ss.str().size());

                            int flag = (--r_size > 0) ? ZMQ_SNDMORE : 0;
                            repSocket.send(msg, flag);
                        }
                    }
                    catch (ParameterError &e) {
                        send_fail_reply(repSocket, e.what());
                    }
                }
                else if (msg.size() == 3 && command == "get") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    std::string parameter((char*) msg[2].data(), msg[2].size());

                    try {
                        std::string value = get_param_(module, parameter);
                        zmq::message_t msg(value.size());
                        memcpy ((void*) msg.data(), value.data(), value.size());
                        repSocket.send(msg, 0);
                    }
                    catch (ParameterError &err) {
                        send_fail_reply(repSocket, err.what());
                    }
                }
                else if (msg.size() == 4 && command == "set") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    std::string parameter((char*) msg[2].data(), msg[2].size());
                    std::string value((char*) msg[3].data(), msg[3].size());

                    try {
                        set_param_(module, parameter, value);
                        send_ok_reply(repSocket);
                    }
                    catch (ParameterError &err) {
                        send_fail_reply(repSocket, err.what());
                    }
                }
                else {
                    send_fail_reply(repSocket,
                            "Unsupported command. commands: list, show, get, set");
                }
            }

            // check if thread is interrupted
            boost::this_thread::interruption_point();
        }
        repSocket.close();
    }
    catch (boost::thread_interrupted&) {}
    catch (zmq::error_t &e) {
        etiLog.level(error) << "ZMQ RC error: " << std::string(e.what());
    }
    catch (std::exception& e) {
        etiLog.level(error) << "ZMQ RC caught exception: " << e.what();
        m_fault = true;
    }
}

#endif

