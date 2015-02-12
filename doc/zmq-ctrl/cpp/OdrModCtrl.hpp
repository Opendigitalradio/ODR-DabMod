/**
 * This is an interface for the zmq ctrl API of the odr-dabmod.
 * The class is intended for clients that wish to control the odr-mod.
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
 **/
#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <zmq.hpp>

class COdrModCtrl
{
	public:
		// ctors
		COdrModCtrl(zmq::context_t *pContext, std::string odrEndpoint,
				unsigned int timeoutMs);
		virtual ~COdrModCtrl();

		// All methods return true if successful, when false check the error
		// string.
		// 
		// IMPORTANT! All methods must be accessed from the same thread.
		//
		// For a detailed description of the various parameters, see
		// example.ini.
		virtual bool Ping(void);
		virtual bool GetDigitalGain(double &gain, std::string &error);
		virtual bool GetTxGain(double &gain, std::string &error);
		virtual bool GetTxFrequency(double &freqHz, std::string &error);
		virtual bool GetMuting(bool &mute, std::string &error);
		virtual bool GetStaticDelay(uint32_t &delayUs, std::string &error);

		virtual bool SetDigitalGain(const double gain, std::string &error);
		virtual bool SetTxGain(const double gain, std::string &error);
		virtual bool SetTxFrequency(const double freqHz, std::string &error);
		virtual bool SetMuting(const bool mute, std::string &error);
		virtual bool SetStaticDelay(const int32_t delayUs, std::string &error);
	
	private:
		// methods
		
		template<typename Type>
		bool DoSet(const std::string module, const std::string parameter,
				const Type value, std::string &error);

		bool ParseSetReply(const std::vector<std::string> &msg, std::string &error);

		template<typename Type>
		bool DoGet(const std::string module, const std::string parameter,
				Type &value, std::string &error);

		template<typename Type>
		bool ParseGetReply(const std::vector<std::string> &msg, Type &value,
				std::string &error);

		bool ConnectSocket(zmq::socket_t *pSocket, const std::string endpoint,
			std::string &error);

		bool SendMessage(zmq::socket_t* pSocket,
				const std::vector<std::string> &message, std::string &error);

		bool RecvAll(zmq::socket_t* pSocket,
				std::vector<std::string> &message, unsigned int timeoutMs,
				std::string &error);

		// data
		zmq::context_t *m_pContext;
		std::string m_odrEndpoint;
		uint32_t m_timeoutMs;
		zmq::socket_t *m_pReqSocket;
};
