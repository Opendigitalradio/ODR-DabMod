/*!
 * This is an implementation for the zmq ctrl API of the odr-dabmod.
 *
 * Copyright (c) 2015 by Jörgen Scott (jorgen.scott@paneda.se)
 *
 * ODR-DabMod is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * ODR-DabMod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \code
 * #include "OdrModCtrl.hpp"
 * #include <zmq.hpp>
 * ...
 *  zmq::context_t ctx;
 *  std::string error;
 *  COdrModCtrl *pCtrl = new COdrModCtrl(&context, // zmq context
 *							"tcp://127.0.0.1:9400", // zmq endpoint
 *							1000); // timeout in milliseconds
 *	if (pCtrl->SetTxGain(50, error))
 *		std::cout << "Tx gain set to 50" << std::endl;
 *	else
 *		std::cout << "An error occured: " << error << std::endl;
 *	delete pCtrl; // destructor will close zmq socket
 *
 * \endcode
 **/
#include "OdrModCtrl.hpp"
#include <sstream>

#define MOD_GAIN "gain"
#define MOD_UHD "uhd"

#define PARAM_DIG_GAIN "digital"
#define PARAM_TX_GAIN "txgain"
#define PARAM_FREQ "freq"
#define PARAM_MUTE "muting"
#define PARAM_STAT_DELAY "staticdelay"

COdrModCtrl::COdrModCtrl(zmq::context_t *pContext, std::string odrEndpoint,
		unsigned int timeoutMs)
{
	m_pContext = pContext;
	m_odrEndpoint = odrEndpoint;
	m_timeoutMs = (uint32_t) timeoutMs;
	m_pReqSocket = NULL;
}

COdrModCtrl::~COdrModCtrl()
{
	if (m_pReqSocket != NULL)
	{
		m_pReqSocket->close();
		delete m_pReqSocket;
	}
}

//// public get methods /////////////////////////////////////////////////////////
bool COdrModCtrl::GetDigitalGain(double &gain, std::string &error)
{
	return DoGet(MOD_GAIN, PARAM_DIG_GAIN, gain, error);
}

bool COdrModCtrl::GetTxGain(double &gain, std::string &error)
{
	return DoGet(MOD_UHD, PARAM_TX_GAIN, gain, error);
}

bool COdrModCtrl::GetTxFrequency(double &freqHz, std::string &error)
{
	return DoGet(MOD_UHD, PARAM_FREQ, freqHz, error);
}

bool COdrModCtrl::GetMuting(bool &mute, std::string &error)
{
	return DoGet(MOD_UHD, PARAM_MUTE, (uint32_t&) mute, error);
}

bool COdrModCtrl::GetStaticDelay(uint32_t &delayUs, std::string &error)
{
	return DoGet(MOD_UHD, PARAM_STAT_DELAY, delayUs, error);
}


//// public set methods /////////////////////////////////////////////////////////

bool COdrModCtrl::Ping()
{
	std::string error;
	if (m_pReqSocket == NULL)
	{
		m_pReqSocket = new zmq::socket_t(*m_pContext, ZMQ_REQ);
		if (!ConnectSocket(m_pReqSocket, m_odrEndpoint, error))
			return false;
	}

	std::vector<std::string> msg;
	msg.push_back("ping");

	// send the message
	if (!SendMessage(m_pReqSocket, msg, error))
	{
		// destroy the socket according to the "Lazy Pirate Pattern" in 
		// the zmq guide
		m_pReqSocket->close();
		delete m_pReqSocket;
		m_pReqSocket = NULL;
		return false;
	}

	// wait for reply
	if (!RecvAll(m_pReqSocket, msg, m_timeoutMs, error))
		return false;

	return true;
}

bool COdrModCtrl::SetDigitalGain(const double gain, std::string &error)
{
	return DoSet(MOD_GAIN, PARAM_DIG_GAIN, gain, error);
}

bool COdrModCtrl::SetTxGain(const double gain, std::string &error)
{
	return DoSet(MOD_UHD, PARAM_TX_GAIN, gain, error);
}

bool COdrModCtrl::SetTxFrequency(const double freqHz, std::string &error)
{
	return DoSet(MOD_UHD, PARAM_FREQ, freqHz, error);
}

bool COdrModCtrl::SetMuting(const bool mute, std::string &error)
{
	return DoSet(MOD_UHD, PARAM_MUTE, mute, error);
}

bool COdrModCtrl::SetStaticDelay(const int32_t delayUs, std::string &error)
{
	return DoSet(MOD_UHD, PARAM_STAT_DELAY, delayUs, error);
}


//// private methods ////////////////////////////////////////////////////////////

template<typename Type>
bool COdrModCtrl::DoSet(const std::string module, const std::string parameter,
		const Type value, std::string &error)
{
	if (m_pReqSocket == NULL)
	{
		m_pReqSocket = new zmq::socket_t(*m_pContext, ZMQ_REQ);
		if (!ConnectSocket(m_pReqSocket, m_odrEndpoint, error))
			return false;
	}

	std::vector<std::string> msg;
	msg.push_back("set");
	msg.push_back(module);
	msg.push_back(parameter);
	std::stringstream ss;
	ss << value;
	msg.push_back(ss.str());

	// send the message
	if (!SendMessage(m_pReqSocket, msg, error))
	{
		// destroy the socket according to the "Lazy Pirate Pattern" in 
		// the zmq guide
		m_pReqSocket->close();
		delete m_pReqSocket;
		m_pReqSocket = NULL;
		return false;
	}

	// wait for reply
	if (!RecvAll(m_pReqSocket, msg, m_timeoutMs, error))
		return false;

	return ParseSetReply(msg, error);
}

bool COdrModCtrl::ParseSetReply(const std::vector<std::string> &msg,
		std::string &error)
{
	error = "";
	if (msg.size() < 1)
		error =  "Bad reply format";
	else if (msg.size() == 1 && msg[0] == "ok")
		return true;
	else if (msg.size() == 2 && msg[0] == "fail")
	{
		error =  msg[1];
		return false;
	}
	else
	{
		error = "Bad reply format";
		return false;
	}
}

template<typename Type>
bool COdrModCtrl::DoGet(const std::string module, const std::string parameter,
		Type &value, std::string &error)
{
	if (m_pReqSocket == NULL)
	{
		m_pReqSocket = new zmq::socket_t(*m_pContext, ZMQ_REQ);
		if (!ConnectSocket(m_pReqSocket, m_odrEndpoint, error))
			return false;
	}

	std::vector<std::string> msg;
	msg.push_back("get");
	msg.push_back(module);
	msg.push_back(parameter);

	// send the message
	if (!SendMessage(m_pReqSocket, msg, error))
	{
		// destroy the socket according to the "Lazy Pirate Pattern" 
		// in the zmq guide
		m_pReqSocket->close();
		delete m_pReqSocket;
		m_pReqSocket = NULL;
		return false;
	}

	// wait for reply
	if (!RecvAll(m_pReqSocket, msg, m_timeoutMs, error))
		return false;

	return ParseGetReply(msg, value, error);
}

template<typename Type>
bool COdrModCtrl::ParseGetReply(const std::vector<std::string> &msg,
		Type &value, std::string &error)
{
	error = "";
	if (msg.size() < 1)
		error =  "Bad reply format";
	else if (msg.size() == 1)
	{
		std::stringstream ss(msg[0]);
		ss >> value;
		return true;
	}
	else if (msg.size() == 2 && msg[0] == "fail")
	{
		error =  msg[1];
		return false;
	}
	else
	{
		error = "Bad reply format";
		return false;
	}
}

bool COdrModCtrl::ConnectSocket(zmq::socket_t *pSocket, const std::string endpoint,
		std::string &error)
{
	error = "";
	try
	{
		int hwm = 1;
		int linger = 0;
		pSocket->setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
		pSocket->setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
		pSocket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
		pSocket->connect(endpoint.c_str());
		return true;
	}
	catch(zmq::error_t &ex)
	{
		error = "Failed to connect: " + endpoint + 
				std::string(". ZMQ: " + std::string(ex.what()));	
		return false;
	}
}

bool COdrModCtrl::SendMessage(zmq::socket_t* pSocket, 
		const std::vector<std::string> &message, std::string &error)
{
	error = "";
	try
	{
		std::vector<std::string>::size_type i = 0;
		for ( ; i < message.size() - 1; i++)
		{
			zmq::message_t zmqMsg(message[i].length());
			memcpy ((void*) zmqMsg.data(), message[i].data(), message[i].length());
			pSocket->send(zmqMsg, ZMQ_SNDMORE);
		}
		zmq::message_t zmqMsg(message[i].length());
		memcpy ((void*) zmqMsg.data(), message[i].data(), message[i].length());
		pSocket->send(zmqMsg, 0);
		return true;
	}
	catch(zmq::error_t &ex)
	{
		error = "ZMQ send error: " + std::string(ex.what());	
		return false;
	}
}

bool COdrModCtrl::RecvAll(zmq::socket_t* pSocket,
		std::vector<std::string> &message, unsigned int timeoutMs,
		std::string &error)
{
	error = "";
	message.clear();

	int more = -1;
	size_t more_size = sizeof(more);
	zmq::pollitem_t pollItems[] = { {*pSocket, 0, ZMQ_POLLIN, 0} };
	zmq::poll(&pollItems[0], 1, timeoutMs);

	while (more != 0)
	{				
		if (pollItems[0].revents & ZMQ_POLLIN)
		{
			zmq::message_t msg;
			pSocket->recv(&msg);
			message.push_back(std::string((char*)msg.data(), msg.size()));
			pSocket->getsockopt(ZMQ_RCVMORE, &more, &more_size);
		}
		else
		{
			error = "Receive timeout";
			return false;
		}
	}

	return true;
}

