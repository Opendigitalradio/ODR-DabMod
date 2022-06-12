/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

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
#include <list>
#include <string>
#include <iostream>
#include <string>
#include <algorithm>

#include "RemoteControl.h"
#if defined(HAVE_ZEROMQ)
    #include "zmq.hpp"
#endif

using namespace std;

RemoteControllerTelnet::~RemoteControllerTelnet()
{
    m_active = false;

    if (m_restarter_thread.joinable()) {
        m_restarter_thread.join();
    }

    if (m_child_thread.joinable()) {
        m_child_thread.join();
    }
}

void RemoteControllerTelnet::restart()
{
    if (m_restarter_thread.joinable()) {
        m_restarter_thread.join();
    }

    m_restarter_thread = std::thread(
            &RemoteControllerTelnet::restart_thread,
            this, 0);
}

RemoteControllable::~RemoteControllable() {
    rcs.remove_controllable(this);
}

std::list<std::string> RemoteControllable::get_supported_parameters() const {
    std::list<std::string> parameterlist;
    for (const auto& param : m_parameters) {
        parameterlist.push_back(param[0]);
    }
    return parameterlist;
}

void RemoteControllers::add_controller(std::shared_ptr<BaseRemoteController> rc) {
    m_controllers.push_back(rc);
}

void RemoteControllers::enrol(RemoteControllable *rc) {
    controllables.push_back(rc);
}

void RemoteControllers::remove_controllable(RemoteControllable *rc) {
    controllables.remove(rc);
}

std::list< std::vector<std::string> > RemoteControllers::get_param_list_values(const std::string& name) {
    RemoteControllable* controllable = get_controllable_(name);

    std::list< std::vector<std::string> > allparams;
    for (auto &param : controllable->get_supported_parameters()) {
        std::vector<std::string> item;
        item.push_back(param);
        try {
            item.push_back(controllable->get_parameter(param));
        }
        catch (const ParameterError &e) {
            item.push_back(std::string("error: ") + e.what());
        }

        allparams.push_back(item);
    }
    return allparams;
}

std::string RemoteControllers::get_param(const std::string& name, const std::string& param) {
    RemoteControllable* controllable = get_controllable_(name);
    return controllable->get_parameter(param);
}

void RemoteControllers::check_faults() {
    for (auto &controller : m_controllers) {
        if (controller->fault_detected()) {
            etiLog.level(warn) <<
                "Detected Remote Control fault, restarting it";
            controller->restart();
        }
    }
}

RemoteControllable* RemoteControllers::get_controllable_(const std::string& name)
{
    auto rc = std::find_if(controllables.begin(), controllables.end(),
            [&](RemoteControllable* r) { return r->get_rc_name() == name; });

    if (rc == controllables.end()) {
        throw ParameterError("Module name unknown");
    }
    else {
        return *rc;
    }
}

void RemoteControllers::set_param(
        const std::string& name,
        const std::string& param,
        const std::string& value)
{
    etiLog.level(info) << "RC: Setting " << name << " " << param
        << " to " << value;
    RemoteControllable* controllable = get_controllable_(name);
    try {
        return controllable->set_parameter(param, value);
    }
    catch (const ios_base::failure& e) {
        etiLog.level(info) << "RC: Failed to set " << name << " " << param
        << " to " << value << ": " << e.what();
        throw ParameterError("Cannot understand value");
    }
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void RemoteControllerTelnet::restart_thread(long)
{
    m_active = false;

    if (m_child_thread.joinable()) {
        m_child_thread.join();
    }

    m_child_thread = std::thread(&RemoteControllerTelnet::process, this, 0);
}

void RemoteControllerTelnet::handle_accept(Socket::TCPSocket&& socket)
{
    const std::string welcome = PACKAGE_NAME " Remote Control CLI\n"
                                "Write 'help' for help.\n"
                                "**********\n";
    const std::string prompt = "> ";

    std::string in_message;

    try {
        etiLog.level(info) << "RC: Accepted";

        socket.sendall(welcome.data(), welcome.size());

        while (m_active and in_message != "quit") {
            socket.sendall(prompt.data(), prompt.size());

            stringstream in_message_stream;

            char last_char = '\0';
            try {
                while (last_char != '\n') {
                    try {
                        auto ret = socket.recv(&last_char, 1, 0, 1000);
                        if (ret == 1) {
                            in_message_stream << last_char;
                        }
                        else {
                            break;
                        }
                    }
                    catch (const Socket::TCPSocket::Timeout&) {
                        if (not m_active) {
                            break;
                        }
                    }
                }
            }
            catch (const Socket::TCPSocket::Interrupted&) {
                in_message_stream.clear();
            }


            if (in_message_stream.str().size() == 0) {
                etiLog.level(info) << "RC: Connection terminated";
                break;
            }

            std::getline(in_message_stream, in_message);

            while (in_message.length() > 0 &&
                    (in_message[in_message.length()-1] == '\r' ||
                     in_message[in_message.length()-1] == '\n')) {
                in_message.erase(in_message.length()-1, 1);
            }

            if (in_message.length() == 0) {
                continue;
            }

            etiLog.level(info) << "RC: Got message '" << in_message << "'";

            dispatch_command(socket, in_message);
        }
        etiLog.level(info) << "RC: Closing socket";
        socket.close();
    }
    catch (const std::exception& e) {
        etiLog.level(error) << "Remote control caught exception: " << e.what();
    }
}

void RemoteControllerTelnet::process(long)
{
    try {
        m_active = true;

        m_socket.listen(m_port, "localhost");

        etiLog.level(info) << "RC: Waiting for connection on port " << m_port;
        while (m_active) {
            auto sock = m_socket.accept(1000);

            if (sock.valid()) {
                handle_accept(move(sock));
                etiLog.level(info) << "RC: Connection closed. Waiting for connection on port " << m_port;
            }
        }
    }
    catch (const runtime_error& e) {
        etiLog.level(warn) << "RC: Encountered error: " << e.what();
    }

    etiLog.level(info) << "RC: Leaving";
    m_fault = true;
}

static std::vector<std::string> tokenise(const std::string& message) {
    stringstream ss(message);
    std::vector<std::string> all_tokens;
    std::string item;

    while (std::getline(ss, item, ' ')) {
        all_tokens.push_back(move(item));
    }
    return all_tokens;
}


void RemoteControllerTelnet::dispatch_command(Socket::TCPSocket& socket, string command)
{
    vector<string> cmd = tokenise(command);

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
            for (auto &controllable : rcs.controllables) {
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
                list< vector<string> > r = rcs.get_param_list_values(cmd[1]);
                for (auto &param_val : r) {
                    ss << param_val[0] << ": " << param_val[1] << endl;
                }
                reply(socket, ss.str());

            }
            catch (const ParameterError &e) {
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
                string r = rcs.get_param(cmd[1], cmd[2]);
                reply(socket, r);
            }
            catch (const ParameterError &e) {
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

                rcs.set_param(cmd[1], cmd[2], new_param_value.str());
                reply(socket, "ok");
            }
            catch (const ParameterError &e) {
                reply(socket, e.what());
            }
            catch (const exception &e) {
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

void RemoteControllerTelnet::reply(Socket::TCPSocket& socket, string message)
{
    stringstream ss;
    ss << message << "\r\n";
    socket.sendall(message.data(), message.size());
}


#if defined(HAVE_ZEROMQ)

RemoteControllerZmq::~RemoteControllerZmq() {
    m_active = false;
    m_fault = false;

    if (m_restarter_thread.joinable()) {
        m_restarter_thread.join();
    }

    if (m_child_thread.joinable()) {
        m_child_thread.join();
    }
}

void RemoteControllerZmq::restart()
{
    if (m_restarter_thread.joinable()) {
        m_restarter_thread.join();
    }

    m_restarter_thread = std::thread(&RemoteControllerZmq::restart_thread, this);
}

// This runs in a separate thread, because
// it would take too long to be done in the main loop
// thread.
void RemoteControllerZmq::restart_thread()
{
    m_active = false;

    if (m_child_thread.joinable()) {
        m_child_thread.join();
    }

    m_child_thread = std::thread(&RemoteControllerZmq::process, this);
}

void RemoteControllerZmq::recv_all(zmq::socket_t& pSocket, std::vector<std::string> &message)
{
    bool more = true;
    do {
        zmq::message_t msg;
        pSocket.recv(msg);
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
    pSocket.send(msg, zmq::send_flags::none);
}

void RemoteControllerZmq::send_fail_reply(zmq::socket_t &pSocket, const std::string &error)
{
    zmq::message_t msg1(4);
    char repCode[4] = {'f', 'a', 'i', 'l'};
    memcpy ((void*) msg1.data(), repCode, 4);
    pSocket.send(msg1, zmq::send_flags::sndmore);

    zmq::message_t msg2(error.length());
    memcpy ((void*) msg2.data(), error.c_str(), error.length());
    pSocket.send(msg2, zmq::send_flags::none);
}

void RemoteControllerZmq::process()
{
    m_fault = false;

    // create zmq reply socket for receiving ctrl parameters
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
        while (m_active) {
            zmq::poll(pollItems, 1, 100);
            std::vector<std::string> msg;

            if (pollItems[0].revents & ZMQ_POLLIN) {
                recv_all(repSocket, msg);

                std::string command((char*)msg[0].data(), msg[0].size());

                if (msg.size() == 1 && command == "ping") {
                    send_ok_reply(repSocket);
                }
                else if (msg.size() == 1 && command == "list") {
                    size_t cohort_size = rcs.controllables.size();
                    for (auto &controllable : rcs.controllables) {
                        std::stringstream ss;
                        ss << "{ \"name\": \"" << controllable->get_rc_name() << "\"," <<
                            " \"params\": { ";

                        list< vector<string> > params = controllable->get_parameter_descriptions();
                        size_t i = 0;
                        for (auto &param : params) {
                            if (i > 0) {
                                ss << ", ";
                            }

                            ss << "\"" << param[0] << "\": " <<
                                "\"" << param[1] << "\"";

                            i++;
                        }

                        ss << " } }";

                        std::string msg_s = ss.str();

                        zmq::message_t zmsg(ss.str().size());
                        memcpy ((void*) zmsg.data(), msg_s.data(), msg_s.size());

                        repSocket.send(zmsg, (--cohort_size > 0) ? zmq::send_flags::sndmore : zmq::send_flags::none);
                    }
                }
                else if (msg.size() == 2 && command == "show") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    try {
                        list< vector<string> > r = rcs.get_param_list_values(module);
                        size_t r_size = r.size();
                        for (auto &param_val : r) {
                            std::stringstream ss;
                            ss << param_val[0] << ": " << param_val[1] << endl;
                            zmq::message_t zmsg(ss.str().size());
                            memcpy(zmsg.data(), ss.str().data(), ss.str().size());

                            repSocket.send(zmsg, (--r_size > 0) ? zmq::send_flags::sndmore : zmq::send_flags::none);
                        }
                    }
                    catch (const ParameterError &err) {
                        send_fail_reply(repSocket, err.what());
                    }
                }
                else if (msg.size() == 3 && command == "get") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    std::string parameter((char*) msg[2].data(), msg[2].size());

                    try {
                        std::string value = rcs.get_param(module, parameter);
                        zmq::message_t zmsg(value.size());
                        memcpy ((void*) zmsg.data(), value.data(), value.size());
                        repSocket.send(zmsg, zmq::send_flags::none);
                    }
                    catch (const ParameterError &err) {
                        send_fail_reply(repSocket, err.what());
                    }
                }
                else if (msg.size() == 4 && command == "set") {
                    std::string module((char*) msg[1].data(), msg[1].size());
                    std::string parameter((char*) msg[2].data(), msg[2].size());
                    std::string value((char*) msg[3].data(), msg[3].size());

                    try {
                        rcs.set_param(module, parameter, value);
                        send_ok_reply(repSocket);
                    }
                    catch (const ParameterError &err) {
                        send_fail_reply(repSocket, err.what());
                    }
                }
                else {
                    send_fail_reply(repSocket,
                            "Unsupported command. commands: list, show, get, set");
                }
            }
        }
        repSocket.close();
    }
    catch (const zmq::error_t &e) {
        etiLog.level(error) << "ZMQ RC error: " << std::string(e.what());
    }
    catch (const std::exception& e) {
        etiLog.level(error) << "ZMQ RC caught exception: " << e.what();
        m_fault = true;
    }
}

#endif
