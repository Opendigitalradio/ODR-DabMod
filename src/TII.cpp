/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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
    ModCodec(),
    RemoteControllable("tii"),
    m_dabmode(dabmode),
    m_conf(tii_config)
{
    PDEBUG("TII::TII(%u) @ %p\n", dabmode, this);

    RC_ADD_PARAMETER(enable, "enable TII [0-1]");
    RC_ADD_PARAMETER(comb, "TII comb number [0-23]");
    RC_ADD_PARAMETER(pattern, "TII pattern number [0-69]");
    RC_ADD_PARAMETER(old_variant, "select old TII variant for old (buggy) receivers [0-1]");

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

    m_Acp.clear();
    m_Acp.resize(m_carriers);
    prepare_pattern();
}

const char* TII::name()
{
    // Calculate name on demand because comb and pattern are
    // modifiable through RC
    std::stringstream ss;
    ss << "TII(c:" << m_conf.comb <<
        " p:" << m_conf.pattern <<
        " vrnt:" << (m_conf.old_variant ? "old" : "new") << ")";
    m_name = ss.str();

    return m_name.c_str();
}


int TII::process(Buffer* dataIn, Buffer* dataOut)
{
    PDEBUG("TII::process(dataOut: %p)\n",
            dataOut);
    if (    (dataIn == NULL) or
            (dataIn->getLength() != m_carriers * sizeof(complexf))) {
        throw TIIError("TII::process input size not valid!");
    }

    dataOut->setLength(m_carriers * sizeof(complexf));
    memset(dataOut->getData(), 0,  dataOut->getLength());

    if (m_conf.enable and m_insert) {
        std::lock_guard<std::mutex> lock(m_enabled_carriers_mutex);
        complexf* in = reinterpret_cast<complexf*>(dataIn->getData());
        complexf* out = reinterpret_cast<complexf*>(dataOut->getData());

        /* Normalise the TII carrier power according to ETSI TR 101 496-3
         * Clause 5.4.2.2 Paragraph 7:
         *
         * > The ratio of carriers in a TII symbol to a normal DAB symbol
         * > is 1:48 for all Modes, so that the signal power in a TII symbol is
         * > 16 dB below the signal power of the other symbols.
         *
         * This is because we only enable 32 out of 1536 carriers, not because
         * every carrier is lower power.
         */
        for (size_t i = 0; i < m_Acp.size(); i++) {
            /* See header file for an explanation of the old variant.
             *
             * A_{c,p}(k) and A_{c,p}(k-1) are never both simultaneously true,
             * so instead of doing the sum inside z_{m,0,k}, we could do
             *
             * if (m_Acp[i]) out[i] = in[i];
             * if (m_Acp[i-1]) out[i] = in[i-1]
             *
             * (Considering only the new variant)
             *
             * To avoid messing with indices, we substitute j = i-1
             *
             * if (m_Acp[i]) out[i] = in[i];
             * if (m_Acp[j]) out[j+1] = in[j]
             *
             * and fuse the two conditionals together:
             */
            if (m_Acp[i]) {
                out[i] = in[i];
                out[i+1] = (m_conf.old_variant ? in[i+1] : in[i]);
            }
        }
    }

    // Align with frames containing the right data (when FC.fp is first quarter)
    m_insert = not m_insert;

    return 1;
}

void TII::enable_carrier(int k)
{
    /* The OFDMGenerator shifts all positive frequencies by one,
     * i.e. index 0 is not the DC component, it's the first positive
     * frequency. Because this is different from the definition of k
     * from the spec, we need to compensate this here.
     *
     * Positive frequencies are k > 0
     */
    int ix = m_carriers/2 + k + (k>=0 ? -1 : 0);

    if (ix < 0 or ix+1 >= (ssize_t)m_Acp.size()) {
        throw TIIError("TII::enable_carrier invalid k!");
    }

    m_Acp[ix] = true;
}

void TII::prepare_pattern()
{
    int comb = m_conf.comb; // Convert from unsigned to signed

    std::lock_guard<std::mutex> lock(m_enabled_carriers_mutex);

    // Clear previous pattern
    for (size_t i = 0; i < m_Acp.size(); i++) {
        m_Acp[i] = false;
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

        for (int k = 385; k <= 768; k++) {
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
    else if (parameter == "old_variant") {
        ss >> m_conf.old_variant;
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
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
    else if (parameter == "old_variant") {
        ss << (m_conf.old_variant ? 1 : 0);
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

