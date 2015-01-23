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

#include "OutputZeroMQ.h"
#include "PcDebug.h"
#include <stdexcept>
#include <string.h>
#include <sstream>

#if defined(HAVE_OUTPUT_ZEROMQ)

OutputZeroMQ::OutputZeroMQ(std::string endpoint, Buffer* dataOut)
    : ModOutput(ModFormat(1), ModFormat(0)),
    m_zmq_context(1),
    m_zmq_pub_sock(m_zmq_context, ZMQ_PUB),
    m_endpoint(endpoint)
{
    PDEBUG("OutputZeroMQ::OutputZeroMQ(%p) @ %p\n", dataOut, this);

    std::stringstream ss;
    ss << "OutputZeroMQ(" << m_endpoint << ")";
    m_name = ss.str();

    m_zmq_pub_sock.bind(m_endpoint.c_str());
}

OutputZeroMQ::~OutputZeroMQ()
{
    PDEBUG("OutputZeroMQ::~OutputZeroMQ() @ %p\n", this);
}

int OutputZeroMQ::process(Buffer* dataIn, Buffer* dataOut)
{
    PDEBUG("OutputZeroMQ::process"
            "(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    m_zmq_pub_sock.send(dataIn->getData(), dataIn->getLength());

    return dataIn->getLength();
}

#endif // HAVE_OUTPUT_ZEROMQ_H

