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

#pragma once
#include <stdio.h>
#include <vector>
#include <map>
#include <stdint.h>

namespace EdiDecoder {
namespace PFT {

using pseq_t = uint16_t;
using findex_t = uint32_t; // findex is a 24-bit value

class Fragment
{
    public:
        // Load the data for one fragment from buf into
        // the Fragment.
        // \returns the number of bytes of useful data found in buf
        // A non-zero return value doesn't imply a valid fragment
        // the isValid() method must be used to verify this.
        size_t loadData(const std::vector<uint8_t> &buf);

        bool isValid() const { return _valid; }
        pseq_t Pseq() const { return _Pseq; }
        findex_t Findex() const { return _Findex; }
        findex_t Fcount() const { return _Fcount; }
        bool FEC() const { return _FEC; }
        uint16_t Plen() const { return _Plen; }
        uint8_t RSk() const { return _RSk; }
        uint8_t RSz() const { return _RSz; }
        const std::vector<uint8_t>& payload() const
            { return _payload; }

        bool checkConsistency(const Fragment& other) const;

    private:
        std::vector<uint8_t> _payload;

        pseq_t _Pseq = 0;
        findex_t _Findex = 0;
        findex_t _Fcount = 0;
        bool _FEC = false;
        bool _Addr = false;
        uint16_t _Plen = 0;
        uint8_t _RSk = 0;
        uint8_t _RSz = 0;
        uint16_t _Source = 0;
        uint16_t _Dest = 0;
        bool _valid = false;
};

/* The AFBuilder collects Fragments and builds an Application Frame
 * out of them. It does error correction if necessary
 */
class AFBuilder
{
    public:
        enum class decode_attempt_result_t {
            yes,   // The AF packet can be build because all fragments are present
            maybe, // RS decoding may correctly decode the AF packet
            no,    // Not enough fragments present to permit RS
        };

        static std::string dar_to_string(decode_attempt_result_t dar) {
            switch (dar) {
                case decode_attempt_result_t::yes: return "y";
                case decode_attempt_result_t::no: return "n";
                case decode_attempt_result_t::maybe: return "m";
            }
            return "?";
        }

        AFBuilder(pseq_t Pseq, findex_t Fcount, size_t lifetime);

        void pushPFTFrag(const Fragment &frag);

        /* Assess if it may be possible to decode this AF packet */
        decode_attempt_result_t canAttemptToDecode() const;

        /* Try to build the AF with received fragments.
         * Apply error correction if necessary (missing packets/CRC errors)
         * \return an empty vector if building the AF is not possible
         */
        std::vector<uint8_t> extractAF(void) const;

        std::pair<findex_t, findex_t>
            numberOfFragments(void) const {
                return {_fragments.size(), _Fcount};
            }

        std::string visualise(void) const;

        /* The user of this instance can keep track of the lifetime of this
         * builder
         */
        size_t lifeTime;

    private:

        // A map from fragment index to fragment
        std::map<findex_t, Fragment> _fragments;

        // cached version of decoded AF packet
        mutable std::vector<uint8_t> _af_packet;

        pseq_t _Pseq;
        findex_t _Fcount;
};

class PFT
{
    public:
        void pushPFTFrag(const Fragment &fragment);

        /* Try to build the AF packet for the next pseq. This might
         * skip one or more pseq according to the maximum delay setting.
         *
         * \return an empty vector if building the AF is not possible
         */
        std::vector<uint8_t> getNextAFPacket(void);

        /* Set the maximum delay in number of AF Packets before we
         * abandon decoding a given pseq.
         */
        void setMaxDelay(size_t num_af_packets);

        /* Enable verbose fprintf */
        void setVerbose(bool enable);

    private:
        void incrementNextPseq(void);

        pseq_t m_next_pseq;
        size_t m_max_delay = 10; // in AF packets

        // Keep one AFBuilder for each Pseq
        std::map<pseq_t, AFBuilder> m_afbuilders;

        bool m_verbose = 0;
};

}

}
