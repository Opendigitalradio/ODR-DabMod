/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#ifndef OUTPUT_ZEROMQ_H
#define OUTPUT_ZEROMQ_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if defined(HAVE_ZEROMQ)

#include "ModOutput.h"
#include "zmq.hpp"

class OutputZeroMQ : public ModOutput
{
    public:
        OutputZeroMQ(std::string endpoint, int type, Buffer* dataOut = NULL);
        virtual ~OutputZeroMQ();
        virtual int process(Buffer* dataIn, Buffer* dataOut);
        const char* name() { return m_name.c_str(); }

    protected:
        int m_type;                   // zmq socket type
        zmq::context_t m_zmq_context; // handle for the zmq context
        zmq::socket_t m_zmq_sock;     // handle for the zmq publisher socket

        std::string m_endpoint;       // On which port to listen: e.g.
                                      // tcp://*:58300

        std::string m_name;
};

#endif // HAVE_ZEROMQ

#endif // OUTPUT_ZEROMQ_H

