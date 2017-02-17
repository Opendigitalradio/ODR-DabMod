/* ------------------------------------------------------------------
 * Copyright (C) 2017 AVT GmbH - Fabien Vercasson
 * Copyright (C) 2017 Matthias P. Braendli
 *                    matthias.braendli@mpb.li
 *
 * http://opendigitalradio.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include <stdio.h>
#include <cassert>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include "crc.h"
#include "PFT.hpp"
#include "Log.h"
#include "buffer_unpack.hpp"
extern "C" {
#include "fec/fec.h"
}

namespace EdiDecoder {
namespace PFT {

using namespace std;

const findex_t NUM_AFBUILDERS_TO_KEEP = 10;

static bool checkCRC(const uint8_t *buf, size_t size)
{
    const uint16_t crc_from_packet = read_16b(buf + size - 2);
    uint16_t crc_calc = 0xffff;
    crc_calc = crc16(crc_calc, buf, size - 2);
    crc_calc ^= 0xffff;

    return crc_from_packet == crc_calc;
}

class FECDecoder {
    public:
        FECDecoder() {
            m_rs_handler = init_rs_char(
                    symsize, gfPoly, firstRoot, primElem, nroots, pad);
        }
        FECDecoder(const FECDecoder& other) = delete;
        FECDecoder& operator=(const FECDecoder& other) = delete;
        ~FECDecoder() {
            free_rs_char(m_rs_handler);
        }

        // return -1 in case of failure, non-negative value if errors
        // were corrected.
        // Known positions of erasures should be given in eras_pos to
        // improve decoding probability. After calling this function
        // eras_pos will contain the positions of the corrected errors.
        int decode(vector<uint8_t> &data, vector<int> &eras_pos) {
            assert(data.size() == N);
            const size_t no_eras = eras_pos.size();

            eras_pos.resize(nroots);
            int num_err = decode_rs_char(m_rs_handler, data.data(),
                    eras_pos.data(), no_eras);
            if (num_err > 0) {
                eras_pos.resize(num_err);
            }
            return num_err;
        }

        // return -1 in case of failure, non-negative value if errors
        // were corrected. No known erasures.
        int decode(vector<uint8_t> &data) {
            assert(data.size() == N);
            int num_err = decode_rs_char(m_rs_handler, data.data(), nullptr, 0);
            return num_err;
        }

    private:
        void* m_rs_handler;

        const int firstRoot = 1; // Discovered by analysing EDI dump
        const int gfPoly = 0x11d;

        // The encoding has to be 255, 207 always, because the chunk has to
        // be padded at the end, and not at the beginning as libfec would
        // do
        const size_t N = 255;
        const size_t K = 207;
        const int primElem = 1;
        const int symsize = 8;
        const size_t nroots = N - K; // For EDI PFT, this must be 48
        const size_t pad = ((1 << symsize) - 1) - N; // is 255-N

};

size_t Fragment::loadData(const std::vector<uint8_t> &buf)
{
    const size_t header_len = 14;
    if (buf.size() < header_len) {
        return 0;
    }

    size_t index = 0;

    // Parse PFT Fragment Header (ETSI TS 102 821 V1.4.1 ch7.1)
    if (not (buf[0] == 'P' and buf[1] == 'F') ) {
        throw invalid_argument("Invalid PFT SYNC bytes");
    }
    index += 2; // Psync

    _Pseq = read_16b(buf.begin()+index); index += 2;
    _Findex = read_24b(buf.begin()+index); index += 3;
    _Fcount = read_24b(buf.begin()+index); index += 3;
    _FEC = unpack1bit(buf[index], 0);
    _Addr = unpack1bit(buf[index], 1);
    _Plen = read_16b(buf.begin()+index) & 0x3FFF; index += 2;

    const size_t required_len = header_len +
        (_FEC ? 1 : 0) +
        (_Addr ? 2 : 0) +
        2; // CRC
    if (buf.size() < required_len) {
        return 0;
    }

    // Optional RS Header
    _RSk = 0;
    _RSz = 0;
    if (_FEC) {
        _RSk = buf[index]; index += 1;
        _RSz = buf[index]; index += 1;
    }

    // Optional transport header
    _Source = 0;
    _Dest = 0;
    if (_Addr) {
        _Source = read_16b(buf.begin()+index); index += 2;
        _Dest = read_16b(buf.begin()+index); index += 2;
    }

    index += 2;
    const bool crc_valid = checkCRC(buf.data(), index);
    const bool buf_has_enough_data = (buf.size() >= index + _Plen);

    if (not buf_has_enough_data) {
        return 0;
    }

    _valid = ((not _FEC) or crc_valid) and buf_has_enough_data;

#if 0
    if (!_valid) {
        stringstream ss;
        ss << "Invalid PF fragment: ";
        if (_FEC) {
            ss << " RSk=" << (uint32_t)_RSk << " RSz=" << (uint32_t)_RSz;
        }

        if (_Addr) {
            ss << " Source=" << _Source << " Dest=" << _Dest;
        }
        etiLog.log(debug, "%s\n", ss.str().c_str());
    }
#endif

    _payload.clear();
    if (_valid) {
        copy( buf.begin()+index,
                buf.begin()+index+_Plen,
                back_inserter(_payload));
        index += _Plen;
    }

    return index;
}


AFBuilder::AFBuilder(pseq_t Pseq, findex_t Fcount, size_t lifetime)
{
    _Pseq = Pseq;
    _Fcount = Fcount;
    assert(lifetime > 0);
    lifeTime = lifetime;
}

void AFBuilder::pushPFTFrag(const Fragment &frag)
{
    if (_Pseq != frag.Pseq() or _Fcount != frag.Fcount()) {
        throw invalid_argument("Invalid PFT fragment Pseq or Fcount");
    }
    const auto Findex = frag.Findex();
    const bool fragment_already_received = _fragments.count(Findex);

    if (not fragment_already_received)
    {
        _fragments[Findex] = frag;
    }
}

bool Fragment::checkConsistency(const Fragment& other) const
{
    /* Consistency check, TS 102 821 Clause 7.3.2.
     *
     * Every PFT Fragment produced from a single AF or RS Packet shall have
     * the same values in all of the PFT Header fields except for the Findex,
     * Plen and HCRC fields.
     */

    return other._Fcount == _Fcount and
        other._FEC == _FEC and
        other._RSk == _RSk and
        other._RSz == _RSz and
        other._Addr == _Addr and
        other._Source == _Source and
        other._Dest == _Dest and

        /* The Plen field of all fragments shall be the s for the initial f-1
         * fragments and s - (L%f) for the final fragment.
         * Note that when Reed Solomon has been used, all fragments will be of
         * length s.
         */
        (_FEC ? other._Plen == _Plen : true);
}


AFBuilder::decode_attempt_result_t AFBuilder::canAttemptToDecode() const
{
    if (_fragments.empty()) {
        return AFBuilder::decode_attempt_result_t::no;
    }

    if (_fragments.size() == _Fcount) {
        return AFBuilder::decode_attempt_result_t::yes;
    }

    /* Check that all fragments are consistent */
    const Fragment& first = _fragments.begin()->second;
    if (not std::all_of(_fragments.begin(), _fragments.end(),
            [&](const pair<int, Fragment>& pair) {
                const Fragment& frag = pair.second;
                return first.checkConsistency(frag) and _Pseq == frag.Pseq();
            }) ) {
        throw invalid_argument("Inconsistent PFT fragments");
    }

    // Calculate the minimum number of fragments necessary to apply FEC.
    // This can't be done with the last fragment that may have a
    // smaller size
    // ETSI TS 102 821 V1.4.1 ch 7.4.4
    auto frag_it = _fragments.begin();
    if (frag_it->second.Fcount() == _Fcount - 1) {
        frag_it++;

        if (frag_it == _fragments.end()) {
            return AFBuilder::decode_attempt_result_t::no;
        }
    }

    const Fragment& frag = frag_it->second;

    if ( frag.FEC() )
    {
        const uint16_t _Plen = frag.Plen();

        /* max number of RS chunks that may have been sent */
        const uint32_t _cmax = (_Fcount*_Plen) / (frag.RSk()+48);
        assert(_cmax > 0);

        /* Receiving _rxmin fragments does not guarantee that decoding
         * will succeed! */
        const uint32_t _rxmin = _Fcount - (_cmax*48)/_Plen;

        if (_fragments.size() >= _rxmin) {
            return AFBuilder::decode_attempt_result_t::maybe;
        }
    }

    return AFBuilder::decode_attempt_result_t::no;
}

std::vector<uint8_t> AFBuilder::extractAF() const
{
    if (not _af_packet.empty()) {
        return _af_packet;
    }

    bool ok = false;

    if (canAttemptToDecode() != AFBuilder::decode_attempt_result_t::no) {

        auto frag_it = _fragments.begin();
        if (frag_it->second.Fcount() == _Fcount - 1) {
            frag_it++;

            if (frag_it == _fragments.end()) {
                throw std::runtime_error("Invalid attempt at extracting AF");
            }
        }

        const Fragment& ref_frag = frag_it->second;
        const auto RSk = ref_frag.RSk();
        const auto RSz = ref_frag.RSz();
        const auto Plen = ref_frag.Plen();

        if ( ref_frag.FEC() )
        {
            const uint32_t cmax = (_Fcount*Plen) / (RSk+48);

            // Keep track of erasures (missing fragments) for
            // every chunk
            map<int, vector<int> > erasures;


            // Assemble fragments into a RS block, immediately
            // deinterleaving it.
            vector<uint8_t> rs_block(Plen * _Fcount);
            for (size_t j = 0; j < _Fcount; j++) {
                const bool fragment_present = _fragments.count(j);
                if (fragment_present) {
                    const auto& fragment = _fragments.at(j).payload();

                    if (j != _Fcount - 1 and fragment.size() != Plen) {
                        throw runtime_error("Incorrect fragment length " +
                                to_string(fragment.size()) + " " +
                                to_string(Plen));
                    }

                    if (j == _Fcount - 1 and fragment.size() > Plen) {
                        throw runtime_error("Incorrect last fragment length " +
                                to_string(fragment.size()) + " " +
                                to_string(Plen));
                    }

                    size_t k = 0;
                    for (; k < fragment.size(); k++) {
                        rs_block[k * _Fcount + j] = fragment[k];
                    }

                    for (; k < Plen; k++) {
                        rs_block[k * _Fcount + j] = 0x00;
                    }
                }
                else {
                    // fill with zeros if fragment is missing
                    for (size_t k = 0; k < Plen; k++) {
                        rs_block[k * _Fcount + j] = 0x00;

                        const size_t chunk_ix = (k * _Fcount + j) / (RSk + 48);
                        const size_t chunk_offset = (k * _Fcount + j) % (RSk + 48);
                        erasures[chunk_ix].push_back(chunk_offset);
                    }
                }
            }

            // The RS block is a concatenation of chunks of RSk bytes + 48 parity
            // followed by RSz padding

            FECDecoder fec;
            for (size_t i = 0; i < cmax; i++) {
                // We need to pad the chunk ourself
                vector<uint8_t> chunk(255);
                const auto& block_begin = rs_block.begin() + (RSk + 48) * i;
                copy(block_begin, block_begin + RSk, chunk.begin());
                // bytes between RSk and 207 are 0x00 already
                copy(block_begin + RSk, block_begin + RSk + 48,
                        chunk.begin() + 207);

                int errors_corrected = -1;
                if (erasures.count(i)) {
                    errors_corrected = fec.decode(chunk, erasures[i]);
                }
                else {
                    errors_corrected = fec.decode(chunk);
                }

                if (errors_corrected == -1) {
                    _af_packet.clear();
                    return {};
                }

#if 0
                if (errors_corrected > 0) {
                    etiLog.log(debug, "Corrected %d errors at ", errors_corrected);
                    for (const auto &index : erasures[i]) {
                        etiLog.log(debug, " %d", index);
                    }
                    etiLog.log(debug, "\n");
                }
#endif

                _af_packet.insert(_af_packet.end(), chunk.begin(), chunk.begin() + RSk);
            }

            _af_packet.resize(_af_packet.size() - RSz);
        }
        else {
            // No FEC: just assemble fragments

            for (size_t j = 0; j < _Fcount; ++j) {
                const bool fragment_present = _fragments.count(j);
                if (fragment_present)
                {
                    const auto& fragment = _fragments.at(j);

                    _af_packet.insert(_af_packet.end(),
                       fragment.payload().begin(),
                       fragment.payload().end());
                }
                else {
                    throw logic_error("Missing fragment");
                }
            }
        }

        // EDI specific, must have a CRC.
        if( _af_packet.size() >= 12 ) {
            ok = checkCRC(_af_packet.data(), _af_packet.size());

            if (not ok) {
                etiLog.log(debug, "Too many errors to reconstruct AF from %zu/%u"
                        " PFT fragments\n", _fragments.size(), _Fcount);
            }
        }
    }

    if (not ok) {
        _af_packet.clear();
    }

    return _af_packet;
}

std::string AFBuilder::visualise() const
{
    stringstream ss;
    ss << "|";
    for (size_t i = 0; i < _Fcount; i++) {
        if (_fragments.count(i)) {
            ss << ".";
        }
        else {
            ss << " ";
        }
    }
    ss << "| " << AFBuilder::dar_to_string(canAttemptToDecode()) << " " << lifeTime;
    return ss.str();
}

void PFT::pushPFTFrag(const Fragment &fragment)
{
    // Start decoding the first pseq we receive. In normal
    // operation without interruptions, the map should
    // never become empty
    if (m_afbuilders.empty()) {
        m_next_pseq = fragment.Pseq();
        etiLog.log(debug,"Initialise next_pseq to %u\n", m_next_pseq);
    }

    if (m_afbuilders.count(fragment.Pseq()) == 0) {
        // The AFBuilder wants to know the lifetime in number of fragments,
        // we know the delay in number of AF packets. Every AF packet
        // is cut into Fcount fragments.
        const size_t lifetime = fragment.Fcount() * m_max_delay;

        // Build the afbuilder in the map in-place
        m_afbuilders.emplace(std::piecewise_construct,
                /* key */
                std::forward_as_tuple(fragment.Pseq()),
                /* builder */
                std::forward_as_tuple(fragment.Pseq(), fragment.Fcount(), lifetime));
    }

    auto& p = m_afbuilders.at(fragment.Pseq());
    p.pushPFTFrag(fragment);

    if (m_verbose) {
        etiLog.log(debug, "Got frag %u:%u, afbuilders: ",
                fragment.Pseq(), fragment.Findex());
        for (const auto &k : m_afbuilders) {
            const bool isNextPseq = (m_next_pseq == k.first);
            etiLog.level(debug) << (isNextPseq ? "->" : "  ") <<
                k.first << " " << k.second.visualise();
        }
    }
}


std::vector<uint8_t> PFT::getNextAFPacket()
{
    if (m_afbuilders.count(m_next_pseq) == 0) {
        if (m_afbuilders.size() > m_max_delay) {
            m_afbuilders.clear();
            etiLog.level(debug) << " Reinit";
        }

        return {};
    }

    auto &builder = m_afbuilders.at(m_next_pseq);

    using dar_t = AFBuilder::decode_attempt_result_t;

    if (builder.canAttemptToDecode() == dar_t::yes) {
        auto afpacket = builder.extractAF();
        assert(not afpacket.empty());
        incrementNextPseq();
        return afpacket;
    }
    else if (builder.canAttemptToDecode() == dar_t::maybe) {
        if (builder.lifeTime > 0) {
            builder.lifeTime--;
        }

        if (builder.lifeTime == 0) {
            // Attempt Reed-Solomon decoding
            auto afpacket = builder.extractAF();

            if (afpacket.empty()) {
                etiLog.log(debug,"pseq %d timed out after RS", m_next_pseq);
            }
            incrementNextPseq();
            return afpacket;
        }
    }
    else {
        if (builder.lifeTime > 0) {
            builder.lifeTime--;
        }

        if (builder.lifeTime == 0) {
            etiLog.log(debug, "pseq %d timed out\n", m_next_pseq);
            incrementNextPseq();
        }
    }

    return {};
}

void PFT::setMaxDelay(size_t num_af_packets)
{
    m_max_delay = num_af_packets;
}

void PFT::setVerbose(bool enable)
{
    m_verbose = enable;
}

void PFT::incrementNextPseq()
{
    if (m_afbuilders.count(m_next_pseq - NUM_AFBUILDERS_TO_KEEP) > 0) {
        m_afbuilders.erase(m_next_pseq - NUM_AFBUILDERS_TO_KEEP);
    }

    m_next_pseq++;
}

}
}
