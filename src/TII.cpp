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


TII::TII(unsigned int dabmode, unsigned int comb, unsigned int pattern) :
    ModCodec(ModFormat(0), ModFormat(0)),
    m_dabmode(dabmode),
    m_comb(comb),
    m_pattern(pattern),
    m_insert(true)
{
    PDEBUG("TII::TII(%u) @ %p\n", dabmode, this);

    std::stringstream ss;
    ss << "TII(comb:" << m_comb << ", pattern:" << m_pattern << ")";
    m_name = ss.str();

    switch (m_dabmode) {
        case 1:
            m_carriers = 1536;

            if (not(0 <= m_pattern and m_pattern <= 69) ) {
                throw std::runtime_error(
                        "TII::TII pattern not valid!");
            }
            break;
        case 2:
            m_carriers = 384;

            if (not(0 <= m_pattern and m_pattern <= 69) ) {
                throw std::runtime_error(
                        "TII::TII pattern not valid!");
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
            throw std::runtime_error(ss_exception.str());
    }

    if (not(0 < m_comb and m_comb <= 23) ) {
        throw std::runtime_error(
                "TII::TII comb not valid!");
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


int TII::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("TII::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    if ((dataIn != NULL) && (dataIn->getLength() != 0)) {
        throw std::runtime_error(
                "TII::process input size not valid!");
    }

    if (m_insert) {
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
    fprintf(stderr, "k = %d\n", k);

    int ix = m_carriers/2 + k;

    if (ix < 0 or ix > (ssize_t)m_dataIn.size()) {
        throw std::runtime_error(
                "TII::enable_carrier invalid k!");
    }

    // TODO power of the carrier ?
    m_dataIn.at(ix) = 1.0;
    m_dataIn.at(ix+1) = 1.0; // TODO verify if +1 is really correct
}

void TII::prepare_pattern() {
    int comb = m_comb; // Convert from unsigned to signed

    // This could be written more efficiently, but since it is
    // not performance-critial, it makes sense to write it
    // in the same way as the specification in
    // ETSI EN 300 401 Clause 14.8
    if (m_dabmode == 1) {
        for (int k = -768; k < -384; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == -768 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = -384; k < -0; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == -384 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = 1; k <= 384; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == 1 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }
        }

        for (int k = 384; k <= 768; k++) {
            for (int b = 0; b < 8; b++) {
                if (    k == 385 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }
        }
    }
    else if (m_dabmode == 2) {
        for (int k = -192; k <= 192; k++) {
            for (int b = 0; b < 4; b++) {
                if (    k == -192 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }

            for (int b = 4; b < 8; b++) {
                if (    k == -191 + 2 * comb + 48 * b and
                        pattern_tm1_2_4[m_pattern][b]) {
                    enable_carrier(k);
                }
            }
        }
    }
    else {
        throw std::runtime_error(
                "TII::TII DAB mode not valid!");
    }


}

#ifdef TII_TEST
int main(int argc, char** argv)
{
    const unsigned int mode = 2;
    const unsigned int comb = 4;
    const unsigned int pattern = 16;
    TII tii(mode, comb, pattern);

    return 0;
}
#endif

