/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2015
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

#include "TII.h"
#include "PcDebug.h"

#include <stdio.h>
#include <stdexcept>
#include <string.h>

typedef std::complex<float> complexf;

/* TII pattern for TM I, II, IV */
const int pattern_tm1_2_4[][8] = { // {{{
    {0,0,0,0,1,1,1,1},
    {0,0,0,1,0,1,1,1},
    {0,0,0,1,1,0,1,1},
    {0,0,0,1,1,1,0,1},
    {0,0,0,1,1,1,1,0},
    {0,0,1,0,0,1,1,1},
    {0,0,1,0,1,0,1,1},
    {0,0,1,0,1,1,0,1},
    {0,0,1,0,1,1,1,0},
    {0,0,1,1,0,0,1,1},
    {0,0,1,1,0,1,0,1},
    {0,0,1,1,0,1,1,0},
    {0,0,1,1,1,0,0,1},
    {0,0,1,1,1,0,1,0},
    {0,0,1,1,1,1,0,0},
    {0,1,0,0,0,1,1,1},
    {0,1,0,0,1,0,1,1},
    {0,1,0,0,1,1,0,1},
    {0,1,0,0,1,1,1,0},
    {0,1,0,1,0,0,1,1},
    {0,1,0,1,0,1,0,1},
    {0,1,0,1,0,1,1,0},
    {0,1,0,1,1,0,0,1},
    {0,1,0,1,1,0,1,0},
    {0,1,0,1,1,1,0,0},
    {0,1,1,0,0,0,1,1},
    {0,1,1,0,0,1,0,1},
    {0,1,1,0,0,1,1,0},
    {0,1,1,0,1,0,0,1},
    {0,1,1,0,1,0,1,0},
    {0,1,1,0,1,1,0,0},
    {0,1,1,1,0,0,0,1},
    {0,1,1,1,0,0,1,0},
    {0,1,1,1,0,1,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,0,0,1,1,1},
    {1,0,0,0,1,0,1,1},
    {1,0,0,0,1,1,0,1},
    {1,0,0,0,1,1,1,0},
    {1,0,0,1,0,0,1,1},
    {1,0,0,1,0,1,0,1},
    {1,0,0,1,0,1,1,0},
    {1,0,0,1,1,0,0,1},
    {1,0,0,1,1,0,1,0},
    {1,0,0,1,1,1,0,0},
    {1,0,1,0,0,0,1,1},
    {1,0,1,0,0,1,0,1},
    {1,0,1,0,0,1,1,0},
    {1,0,1,0,1,0,0,1},
    {1,0,1,0,1,0,1,0},
    {1,0,1,0,1,1,0,0},
    {1,0,1,1,0,0,0,1},
    {1,0,1,1,0,0,1,0},
    {1,0,1,1,0,1,0,0},
    {1,0,1,1,1,0,0,0},
    {1,1,0,0,0,0,1,1},
    {1,1,0,0,0,1,0,1},
    {1,1,0,0,0,1,1,0},
    {1,1,0,0,1,0,0,1},
    {1,1,0,0,1,0,1,0},
    {1,1,0,0,1,1,0,0},
    {1,1,0,1,0,0,0,1},
    {1,1,0,1,0,0,1,0},
    {1,1,0,1,0,1,0,0},
    {1,1,0,1,1,0,0,0},
    {1,1,1,0,0,0,0,1},
    {1,1,1,0,0,0,1,0},
    {1,1,1,0,0,1,0,0},
    {1,1,1,0,1,0,0,0},
    {1,1,1,1,0,0,0,0} }; // }}}

TII::TII(unsigned int dabmode, tii_config_t& tii_config) :
    ModCodec(ModFormat(0), ModFormat(0)),
    RemoteControllable("tii"),
    m_dabmode(dabmode),
    m_conf(tii_config),
    m_insert(true)
{
    PDEBUG("TII::TII(%u) @ %p\n", dabmode, this);

    RC_ADD_PARAMETER(enable, "enable TII [0-1]");
    RC_ADD_PARAMETER(comb, "TII comb number [0-23]");
    RC_ADD_PARAMETER(pattern, "TII pattern number [0-69]");

    switch (m_dabmode) {
        case 1:
            m_carriers = 1536;

            if (not(0 <= m_conf.pattern and m_conf.pattern <= 69) ) {
                throw TIIError("TII::TII pattern not valid!");
            }
            break;
        case 2:
            m_carriers = 384;

            if (not(0 <= m_conf.pattern and m_conf.pattern <= 69) ) {
                throw TIIError("TII::TII pattern not valid!");
            }
            break;
        /* unsupported
        case 3:
            m_carriers = 192;
            break;
        case 4:
            d_dabmode = 0;
        case 0:
        */
        default:
            std::stringstream ss_exception;
            ss_exception <<
                    "TII::TII DAB mode " << m_dabmode << " not valid!";
            throw TIIError(ss_exception.str());
    }

    if (not(0 <= m_conf.comb and m_conf.comb <= 23) ) {
        throw TIIError("TII::TII comb not valid!");
    }

    m_dataIn.clear();
    m_dataIn.resize(m_carriers);
    prepare_pattern();

    myOutputFormat.size(m_carriers * sizeof(complexf));
}


TII::~TII()
{
    PDEBUG("TII::~TII() @ %p\n", this);
}

const char* TII::name()
{
    // Calculate name on demand because comb and pattern are
    // modifiable through RC
    std::stringstream ss;
    ss << "TII(comb:" << m_conf.comb << ", pattern:" << m_conf.pattern << ")";
    m_name = ss.str();

    return m_name.c_str();
}


int TII::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("TII::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    if ((dataIn != NULL) && (dataIn->getLength() != 0)) {
        throw TIIError("TII::process input size not valid!");
    }

    if (m_conf.enable and m_insert) {
        boost::mutex::scoped_lock lock(m_dataIn_mutex);
        dataOut->setData(&m_dataIn[0], m_carriers * sizeof(complexf));
    }
    else {
        dataOut->setLength(m_carriers * sizeof(complexf));
        bzero(dataOut->getData(), dataOut->getLength());
    }

    // TODO wrong! Must align with frames containing the right data
    m_insert = not m_insert;

    return 1;
}

void TII::enable_carrier(int k) {
    int ix = m_carriers/2 + k;

    if (ix < 0 or ix+1 >= (ssize_t)m_dataIn.size()) {
        throw TIIError("TII::enable_carrier invalid k!");
    }

    // TODO power of the carrier ?
    m_dataIn.at(ix) = 1.0;
    m_dataIn.at(ix+1) = 1.0; // TODO verify if +1 is really correct
}

void TII::prepare_pattern() {
    int comb = m_conf.comb; // Convert from unsigned to signed

    boost::mutex::scoped_lock lock(m_dataIn_mutex);

    // Clear previous pattern
    for (size_t i = 0; i < m_dataIn.size(); i++) {
        m_dataIn[i] = 0.0;
    }

    // This could be written more efficiently, but since it is
    // not performance-critial, it makes sense to write it
    // in the same way as the specification in
    // ETSI EN 300 401 Clause 14.8
    if (m_dabmode == 1) {
        for (int k = -768; k < -384; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == -768 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = -384; k < -0; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == -384 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = 1; k <= 384; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == 1 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = 384; k <= 768; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == 385 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }
        }
    }
    else if (m_dabmode == 2) {
        for (int k = -192; k <= 192; k++) {
            for (int b = 0; b < 4; b++) {
                if (    k == -192 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }

            for (int b = 4; b < 8; b++) {
                if (    k == -191 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_conf.pattern][b]) {
                    enable_carrier(k);
                }
            }
        }
    }
    else {
        throw TIIError("TII::TII DAB mode not valid!");
    }
}

void TII::set_parameter(const std::string& parameter, const std::string& value)
{
    using namespace std;
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "enable") {
        ss >> m_conf.enable;
    }
    else if (parameter == "pattern") {
        int new_pattern;
        ss >> new_pattern;
        if (    (m_dabmode == 1 or m_dabmode == 2) and
                not(0 <= new_pattern and new_pattern <= 69) ) {
            throw TIIError("TII pattern not valid!");
        }
        m_conf.pattern = new_pattern;
        prepare_pattern();
    }
    else if (parameter == "comb") {
        int new_comb;
        ss >> new_comb;
        if (not(0 <= new_comb and new_comb <= 23) ) {
            throw TIIError("TII comb not valid!");
        }
        m_conf.comb = new_comb;
        prepare_pattern();
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const std::string TII::get_parameter(const std::string& parameter) const
{
    using namespace std;
    stringstream ss;
    if (parameter == "enable") {
        ss << (m_conf.enable ? 1 : 0);
    }
    else if (parameter == "pattern") {
        ss << m_conf.pattern;
    }
    else if (parameter == "comb") {
        ss << m_conf.comb;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

