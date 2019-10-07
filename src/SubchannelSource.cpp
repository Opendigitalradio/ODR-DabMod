/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "SubchannelSource.h"
#include "PcDebug.h"
#include "Log.h"

#include <string>
#include <stdexcept>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>


#define P1  0xc8888888
#define P2  0xc888c888
#define P3  0xc8c8c888
#define P4  0xc8c8c8c8
#define P5  0xccc8c8c8
#define P6  0xccc8ccc8
#define P7  0xccccccc8
#define P8  0xcccccccc
#define P9  0xeccccccc
#define P10 0xeccceccc
#define P11 0xecececcc
#define P12 0xecececec
#define P13 0xeeececec
#define P14 0xeeeceeec
#define P15 0xeeeeeeec
#define P16 0xeeeeeeee
#define P17 0xfeeeeeee
#define P18 0xfeeefeee
#define P19 0xfefefeee
#define P20 0xfefefefe
#define P21 0xfffefefe
#define P22 0xfffefffe
#define P23 0xfffffffe
#define P24 0xffffffff


const std::vector<PuncturingRule>& SubchannelSource::get_rules() const
{
    return d_puncturing_rules;
}


SubchannelSource::SubchannelSource(
            uint16_t sad,
            uint16_t stl,
            uint8_t tpl
            ) :
    ModInput(),
    d_start_address(sad),
    d_framesize(stl * 8),
    d_protection(tpl)
{
    PDEBUG("SubchannelSource::SubchannelSource(...) @ %p\n", this);
    PDEBUG("  Start address: %zu\n", d_start_address);
    PDEBUG("  Framesize: %zu\n", d_framesize);
    PDEBUG("  Protection: %zu\n", d_protection);
    if (protectionForm()) {
        if (protectionOption() == 0) {
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(
                        ((6 * bitrate() / 8) - 3) * 16, P24);
                d_puncturing_rules.emplace_back(3 * 16, P23);
                break;
            case 2:
                if (bitrate() == 8) {
                    d_puncturing_rules.emplace_back(5 * 16, P13);
                    d_puncturing_rules.emplace_back(1 * 16, P12);
                } else {
                    d_puncturing_rules.emplace_back(
                            ((2 * bitrate() / 8) - 3) * 16, P14);
                    d_puncturing_rules.emplace_back(
                            ((4 * bitrate() / 8) + 3) * 16, P13);
                }
                break;
            case 3:
                d_puncturing_rules.emplace_back(
                        ((6 * bitrate() / 8) - 3) * 16, P8);
                d_puncturing_rules.emplace_back(3 * 16, P7);
                break;
            case 4:
                d_puncturing_rules.emplace_back(
                        ((4 * bitrate() / 8) - 3) * 16, P3);
                d_puncturing_rules.emplace_back(
                        ((2 * bitrate() / 8) + 3) * 16, P2);
                break;
            default:
                fprintf(stderr,
                        "Protection form(%zu), option(%zu) and level(%zu)\n",
                        protectionForm(), protectionOption(), protectionLevel());
                fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", tpl, tpl);
                throw std::runtime_error("SubchannelSource::SubchannelSource "
                        "unknown protection level!");
            }
        } else if (protectionOption() == 1) {
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(
                            ((24 * bitrate() / 32) - 3) * 16, P10);
                d_puncturing_rules.emplace_back(
                            3 * 16, P9);
                break;
            case 2:
                d_puncturing_rules.emplace_back(
                            ((24 * bitrate() / 32) - 3) * 16, P6);
                d_puncturing_rules.emplace_back(
                            3 * 16, P5);
                break;
            case 3:
                d_puncturing_rules.emplace_back(
                            ((24 * bitrate() / 32) - 3) * 16, P4);
                d_puncturing_rules.emplace_back(
                            3 * 16, P3);
                break;
            case 4:
                d_puncturing_rules.emplace_back(
                            ((24 * bitrate() / 32) - 3) * 16, P2);
                d_puncturing_rules.emplace_back(
                            3 * 16, P1);
                break;
            default:
                fprintf(stderr,
                        "Protection form(%zu), option(%zu) and level(%zu)\n",
                        protectionForm(), protectionOption(), protectionLevel());
                fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", tpl, tpl);
                throw std::runtime_error("SubchannelSource::SubchannelSource "
                        "unknown protection level!");
            }
        } else {
            fprintf(stderr,
                    "Protection form(%zu), option(%zu) and level(%zu)\n",
                    protectionForm(), protectionOption(), protectionLevel());
            fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", tpl, tpl);
            throw std::runtime_error("SubchannelSource::SubchannelSource "
                    "unknown protection option!");
        }
    }
    else {
        bool rule_error = false;
        switch (bitrate()) {
        case 32:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(3  * 16, P24);
                d_puncturing_rules.emplace_back(5  * 16, P17);
                d_puncturing_rules.emplace_back(13 * 16, P12);
                d_puncturing_rules.emplace_back(3  * 16, P17);
                break;
            case 2:
                d_puncturing_rules.emplace_back(3  * 16, P22);
                d_puncturing_rules.emplace_back(4  * 16, P13);
                d_puncturing_rules.emplace_back(14 * 16, P8 );
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(3  * 16, P15);
                d_puncturing_rules.emplace_back(4  * 16, P9 );
                d_puncturing_rules.emplace_back(14 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P8 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(3  * 16, P11);
                d_puncturing_rules.emplace_back(3  * 16, P6 );
                d_puncturing_rules.emplace_back(18 * 16, P5 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(3  * 16, P5 );
                d_puncturing_rules.emplace_back(4  * 16, P3 );
                d_puncturing_rules.emplace_back(17 * 16, P2 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 48:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(3  * 16, P24);
                d_puncturing_rules.emplace_back(5  * 16, P18);
                d_puncturing_rules.emplace_back(25 * 16, P13);
                d_puncturing_rules.emplace_back(3  * 16, P18);
                break;
            case 2:
                d_puncturing_rules.emplace_back(3  * 16, P24);
                d_puncturing_rules.emplace_back(4  * 16, P14);
                d_puncturing_rules.emplace_back(26 * 16, P8 );
                d_puncturing_rules.emplace_back(3  * 16, P15);
                break;
            case 3:
                d_puncturing_rules.emplace_back(3  * 16, P15);
                d_puncturing_rules.emplace_back(4  * 16, P10);
                d_puncturing_rules.emplace_back(26 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                d_puncturing_rules.emplace_back(4  * 16, P6 );
                d_puncturing_rules.emplace_back(26 * 16, P4 );
                d_puncturing_rules.emplace_back(3  * 16, P6 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(4  * 16, P5 );
                d_puncturing_rules.emplace_back(3  * 16, P4 );
                d_puncturing_rules.emplace_back(26 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P3 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 56:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.emplace_back(6  * 16, P23);
                d_puncturing_rules.emplace_back(10 * 16, P13);
                d_puncturing_rules.emplace_back(23 * 16, P8 );
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(6  * 16, P16);
                d_puncturing_rules.emplace_back(12 * 16, P7 );
                d_puncturing_rules.emplace_back(21 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(6  * 16, P9 );
                d_puncturing_rules.emplace_back(10 * 16, P6 );
                d_puncturing_rules.emplace_back(23 * 16, P4 );
                d_puncturing_rules.emplace_back(3  * 16, P5 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(6  * 16, P5 );
                d_puncturing_rules.emplace_back(10 * 16, P4 );
                d_puncturing_rules.emplace_back(23 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P3 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 64:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(6  * 16, P24);
                d_puncturing_rules.emplace_back(11 * 16, P18);
                d_puncturing_rules.emplace_back(28 * 16, P12);
                d_puncturing_rules.emplace_back(3  * 16, P18);
                break;
            case 2:
                d_puncturing_rules.emplace_back(6  * 16, P23);
                d_puncturing_rules.emplace_back(10 * 16, P13);
                d_puncturing_rules.emplace_back(29 * 16, P8 );
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(6  * 16, P16);
                d_puncturing_rules.emplace_back(12 * 16, P8 );
                d_puncturing_rules.emplace_back(27 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(6  * 16, P11);
                d_puncturing_rules.emplace_back(9  * 16, P6 );
                d_puncturing_rules.emplace_back(33 * 16, P5 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(6  * 16, P5 );
                d_puncturing_rules.emplace_back(9  * 16, P3 );
                d_puncturing_rules.emplace_back(31 * 16, P2 );
                d_puncturing_rules.emplace_back(2  * 16, P3 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 80:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(6  * 16, P24);
                d_puncturing_rules.emplace_back(10 * 16, P17);
                d_puncturing_rules.emplace_back(41 * 16, P12);
                d_puncturing_rules.emplace_back(3  * 16, P18);
                break;
            case 2:
                d_puncturing_rules.emplace_back(6  * 16, P23);
                d_puncturing_rules.emplace_back(10 * 16, P13);
                d_puncturing_rules.emplace_back(41 * 16, P8 );
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(6  * 16, P16);
                d_puncturing_rules.emplace_back(11 * 16, P8 );
                d_puncturing_rules.emplace_back(40 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P7 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(6  * 16, P11);
                d_puncturing_rules.emplace_back(10 * 16, P6 );
                d_puncturing_rules.emplace_back(41 * 16, P5 );
                d_puncturing_rules.emplace_back(3  * 16, P6 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(6  * 16, P6 );
                d_puncturing_rules.emplace_back(10 * 16, P3 );
                d_puncturing_rules.emplace_back(41 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P3 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 96:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(6  * 16, P24);
                d_puncturing_rules.emplace_back(13 * 16, P18);
                d_puncturing_rules.emplace_back(50 * 16, P13);
                d_puncturing_rules.emplace_back(3  * 16, P19);
                break;
            case 2:
                d_puncturing_rules.emplace_back(6  * 16, P22);
                d_puncturing_rules.emplace_back(10 * 16, P12);
                d_puncturing_rules.emplace_back(53 * 16, P9 );
                d_puncturing_rules.emplace_back(3  * 16, P12);
                break;
            case 3:
                d_puncturing_rules.emplace_back(6  * 16, P16);
                d_puncturing_rules.emplace_back(12 * 16, P9 );
                d_puncturing_rules.emplace_back(51 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 4:
                d_puncturing_rules.emplace_back(7  * 16, P9 );
                d_puncturing_rules.emplace_back(10 * 16, P6 );
                d_puncturing_rules.emplace_back(52 * 16, P4 );
                d_puncturing_rules.emplace_back(3  * 16, P6 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(7  * 16, P5 );
                d_puncturing_rules.emplace_back(9  * 16, P4 );
                d_puncturing_rules.emplace_back(53 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P4 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 112:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P23);
                d_puncturing_rules.emplace_back(21 * 16, P12);
                d_puncturing_rules.emplace_back(49 * 16, P9 );
                d_puncturing_rules.emplace_back(3  * 16, P14);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(23 * 16, P8 );
                d_puncturing_rules.emplace_back(47 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P9 );
                d_puncturing_rules.emplace_back(21 * 16, P6 );
                d_puncturing_rules.emplace_back(49 * 16, P4 );
                d_puncturing_rules.emplace_back(3  * 16, P8 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(14 * 16, P5 );
                d_puncturing_rules.emplace_back(17 * 16, P4 );
                d_puncturing_rules.emplace_back(50 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P5 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 128:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(20 * 16, P17);
                d_puncturing_rules.emplace_back(62 * 16, P13);
                d_puncturing_rules.emplace_back(3  * 16, P19);
                break;
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P22);
                d_puncturing_rules.emplace_back(21 * 16, P12);
                d_puncturing_rules.emplace_back(61 * 16, P9 );
                d_puncturing_rules.emplace_back(3  * 16, P14);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(22 * 16, P9 );
                d_puncturing_rules.emplace_back(60 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P11);
                d_puncturing_rules.emplace_back(21 * 16, P6 );
                d_puncturing_rules.emplace_back(61 * 16, P5 );
                d_puncturing_rules.emplace_back(3  * 16, P7 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(12 * 16, P5 );
                d_puncturing_rules.emplace_back(19 * 16, P3 );
                d_puncturing_rules.emplace_back(62 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P4 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 160:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(22 * 16, P18);
                d_puncturing_rules.emplace_back(84 * 16, P12);
                d_puncturing_rules.emplace_back(3  * 16, P19);
                break;
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P22);
                d_puncturing_rules.emplace_back(21 * 16, P11);
                d_puncturing_rules.emplace_back(85 * 16, P9 );
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(24 * 16, P8 );
                d_puncturing_rules.emplace_back(82 * 16, P6 );
                d_puncturing_rules.emplace_back(3  * 16, P11);
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P11);
                d_puncturing_rules.emplace_back(23 * 16, P6 );
                d_puncturing_rules.emplace_back(83 * 16, P5 );
                d_puncturing_rules.emplace_back(3  * 16, P9 );
                break;
            case 5:
                d_puncturing_rules.emplace_back(11 * 16, P5 );
                d_puncturing_rules.emplace_back(19 * 16, P4 );
                d_puncturing_rules.emplace_back(87 * 16, P2 );
                d_puncturing_rules.emplace_back(3  * 16, P4 );
                break;
            default:
                rule_error = true;
            }
            break;
        case 192:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(21 * 16, P20);
                d_puncturing_rules.emplace_back(109 * 16, P13);
                d_puncturing_rules.emplace_back(3  * 16, P24);
                break;
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P22);
                d_puncturing_rules.emplace_back(20 * 16, P13);
                d_puncturing_rules.emplace_back(110 * 16, P9);
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(24 * 16, P10);
                d_puncturing_rules.emplace_back(106 * 16, P6);
                d_puncturing_rules.emplace_back(3  * 16, P11);
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P10);
                d_puncturing_rules.emplace_back(22 * 16, P6);
                d_puncturing_rules.emplace_back(108 * 16, P4);
                d_puncturing_rules.emplace_back(3  * 16, P9);
                break;
            case 5:
                d_puncturing_rules.emplace_back(11 * 16, P6);
                d_puncturing_rules.emplace_back(20 * 16, P4);
                d_puncturing_rules.emplace_back(110 * 16, P2);
                d_puncturing_rules.emplace_back(3  * 16, P5);
                break;
            default:
                rule_error = true;
            }
            break;
        case 224:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(24 * 16, P20);
                d_puncturing_rules.emplace_back(130 * 16, P12);
                d_puncturing_rules.emplace_back(3  * 16, P20);
                break;
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(22 * 16, P16);
                d_puncturing_rules.emplace_back(132 * 16, P10);
                d_puncturing_rules.emplace_back(3  * 16, P15);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(20 * 16, P10);
                d_puncturing_rules.emplace_back(134 * 16, P7);
                d_puncturing_rules.emplace_back(3  * 16, P9);
                break;
            case 4:
                d_puncturing_rules.emplace_back(12 * 16, P12);
                d_puncturing_rules.emplace_back(26 * 16, P8);
                d_puncturing_rules.emplace_back(127 * 16, P4);
                d_puncturing_rules.emplace_back(3  * 16, P11);
                break;
            case 5:
                d_puncturing_rules.emplace_back(12 * 16, P8);
                d_puncturing_rules.emplace_back(22 * 16, P6);
                d_puncturing_rules.emplace_back(131 * 16, P2);
                d_puncturing_rules.emplace_back(3  * 16, P6);
                break;
            default:
                rule_error = true;
            }
            break;
        case 256:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(26 * 16, P19);
                d_puncturing_rules.emplace_back(152 * 16, P14);
                d_puncturing_rules.emplace_back(3  * 16, P18);
                break;
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(22 * 16, P14);
                d_puncturing_rules.emplace_back(156 * 16, P10);
                d_puncturing_rules.emplace_back(3  * 16, P13);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(27 * 16, P10);
                d_puncturing_rules.emplace_back(151 * 16, P7);
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P12);
                d_puncturing_rules.emplace_back(24 * 16, P9);
                d_puncturing_rules.emplace_back(154 * 16, P5);
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 5:
                d_puncturing_rules.emplace_back(11 * 16, P6);
                d_puncturing_rules.emplace_back(24 * 16, P5);
                d_puncturing_rules.emplace_back(154 * 16, P2);
                d_puncturing_rules.emplace_back(3  * 16, P5);
                break;
            default:
                rule_error = true;
            }
            break;
        case 320:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.emplace_back(11 * 16, P24);
                d_puncturing_rules.emplace_back(26 * 16, P17);
                d_puncturing_rules.emplace_back(200 * 16, P9 );
                d_puncturing_rules.emplace_back(3  * 16, P17);
                break;
            case 4:
                d_puncturing_rules.emplace_back(11 * 16, P13);
                d_puncturing_rules.emplace_back(25 * 16, P9);
                d_puncturing_rules.emplace_back(201 * 16, P5);
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 5:
                d_puncturing_rules.emplace_back(11 * 16, P8);
                d_puncturing_rules.emplace_back(26 * 16, P5);
                d_puncturing_rules.emplace_back(200 * 16, P2);
                d_puncturing_rules.emplace_back(3  * 16, P6);
                break;
            default:
                rule_error = true;
            }
            break;
        case 384:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.emplace_back(12 * 16, P24);
                d_puncturing_rules.emplace_back(28 * 16, P20);
                d_puncturing_rules.emplace_back(245 * 16, P14);
                d_puncturing_rules.emplace_back(3  * 16, P23);
                break;
            case 3:
                d_puncturing_rules.emplace_back(11 * 16, P16);
                d_puncturing_rules.emplace_back(24 * 16, P9);
                d_puncturing_rules.emplace_back(250 * 16, P7);
                d_puncturing_rules.emplace_back(3  * 16, P10);
                break;
            case 5:
                d_puncturing_rules.emplace_back(11 * 16, P8);
                d_puncturing_rules.emplace_back(27 * 16, P6);
                d_puncturing_rules.emplace_back(247 * 16, P2);
                d_puncturing_rules.emplace_back(3  * 16, P7);
                break;
            default:
                rule_error = true;
            }
            break;
        default:
            rule_error = true;
        }

        if (rule_error) {
            etiLog.log(error, " Protection: UEP-%zu @ %zukb/s\n",
                    protectionLevel(), bitrate());
            throw std::runtime_error("ubchannelSource "
                    "UEP puncturing rules do not exist!");
        }
    }
}

size_t SubchannelSource::startAddress() const
{
    return d_start_address;
}

size_t SubchannelSource::framesize() const
{
    return d_framesize;
}


size_t SubchannelSource::framesizeCu() const
{
    size_t framesizeCu = 0;

    if (protectionForm()) { // Long form
        if ((d_protection >> 2) & 0x07) { // Option
            switch (d_protection & 0x03) {
            case 0:
                framesizeCu = (bitrate() / 32) * 27;
                break;
            case 1:
                framesizeCu = (bitrate() / 32) * 21;
                break;
            case 2:
                framesizeCu = (bitrate() / 32) * 18;
                break;
            case 3:
                framesizeCu = (bitrate() / 32) * 15;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
        } else {
            switch (d_protection & 0x03) {
            case 0:
                framesizeCu = (bitrate() / 8) * 12;
                break;
            case 1:
                framesizeCu = (bitrate() / 8) * 8;
                break;
            case 2:
                framesizeCu = (bitrate() / 8) * 6;
                break;
            case 3:
                framesizeCu = (bitrate() / 8) * 4;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
        }
    } else {   // Short form
        switch (bitrate()) {
        case 32:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 35;
                break;
            case 2:
                framesizeCu = 29;
                break;
            case 3:
                framesizeCu = 24;
                break;
            case 4:
                framesizeCu = 21;
                break;
            case 5:
                framesizeCu = 16;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 48:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 52;
                break;
            case 2:
                framesizeCu = 42;
                break;
            case 3:
                framesizeCu = 35;
                break;
            case 4:
                framesizeCu = 29;
                break;
            case 5:
                framesizeCu = 24;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 56:
            switch (protectionLevel()) {
            case 2:
                framesizeCu = 52;
                break;
            case 3:
                framesizeCu = 42;
                break;
            case 4:
                framesizeCu = 35;
                break;
            case 5:
                framesizeCu = 29;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 64:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 70;
                break;
            case 2:
                framesizeCu = 58;
                break;
            case 3:
                framesizeCu = 48;
                break;
            case 4:
                framesizeCu = 42;
                break;
            case 5:
                framesizeCu = 32;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 80:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 84;
                break;
            case 2:
                framesizeCu = 70;
                break;
            case 3:
                framesizeCu = 58;
                break;
            case 4:
                framesizeCu = 52;
                break;
            case 5:
                framesizeCu = 40;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 96:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 104;
                break;
            case 2:
                framesizeCu = 84;
                break;
            case 3:
                framesizeCu = 70;
                break;
            case 4:
                framesizeCu = 58;
                break;
            case 5:
                framesizeCu = 48;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 112:
            switch (protectionLevel()) {
            case 2:
                framesizeCu = 104;
                break;
            case 3:
                framesizeCu = 84;
                break;
            case 4:
                framesizeCu = 70;
                break;
            case 5:
                framesizeCu = 58;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 128:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 140;
                break;
            case 2:
                framesizeCu = 116;
                break;
            case 3:
                framesizeCu = 96;
                break;
            case 4:
                framesizeCu = 84;
                break;
            case 5:
                framesizeCu = 64;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 160:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 168;
                break;
            case 2:
                framesizeCu = 140;
                break;
            case 3:
                framesizeCu = 116;
                break;
            case 4:
                framesizeCu = 104;
                break;
            case 5:
                framesizeCu = 80;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 192:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 208;
                break;
            case 2:
                framesizeCu = 168;
                break;
            case 3:
                framesizeCu = 140;
                break;
            case 4:
                framesizeCu = 116;
                break;
            case 5:
                framesizeCu = 96;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 224:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 232;
                break;
            case 2:
                framesizeCu = 208;
                break;
            case 3:
                framesizeCu = 168;
                break;
            case 4:
                framesizeCu = 140;
                break;
            case 5:
                framesizeCu = 116;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 256:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 280;
                break;
            case 2:
                framesizeCu = 232;
                break;
            case 3:
                framesizeCu = 192;
                break;
            case 4:
                framesizeCu = 168;
                break;
            case 5:
                framesizeCu = 128;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 320:
            switch (protectionLevel()) {
            case 2:
                framesizeCu = 280;
                break;
            case 4:
                framesizeCu = 208;
                break;
            case 5:
                framesizeCu = 160;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        case 384:
            switch (protectionLevel()) {
            case 1:
                framesizeCu = 416;
                break;
            case 3:
                framesizeCu = 280;
                break;
            case 5:
                framesizeCu = 192;
                break;
            default:
                framesizeCu = 0xffff;
                break;
            }
            break;
        default:
            framesizeCu = 0xffff;
            break;
        }
    }

    if (framesizeCu == 0) {
        fprintf(stderr, " Protection %zu @ %zu kb/s\n",
                protectionLevel(), bitrate());
        throw std::runtime_error("SubchannelSource::framesizeCu protection "
                "not yet coded!");
    }
    if (framesizeCu == 0xffff) {
        fprintf(stderr, " Protection %zu @ %zu kb/s\n",
                protectionLevel(), bitrate());
        throw std::runtime_error("SubchannelSource::framesizeCu invalid "
                "protection!");
    }

    return framesizeCu;
}


size_t SubchannelSource::bitrate() const
{
    return d_framesize / 3;
}


size_t SubchannelSource::protection() const
{
    return d_protection;
}


size_t SubchannelSource::protectionForm() const
{
    return (d_protection >> 5) & 1;
}


size_t SubchannelSource::protectionLevel() const
{
    if (protectionForm()) { // Long form
        return (d_protection & 0x3) + 1;
    }   // Short form
    return (d_protection & 0x7) + 1;
}


size_t SubchannelSource::protectionOption() const
{
    if (protectionForm()) { // Long form
        return (d_protection >> 2) & 0x7;
    }   // Short form
    return 0;
}

void SubchannelSource::loadSubchannelData(Buffer&& data)
{
    d_buffer = std::move(data);
}

int SubchannelSource::process(Buffer* outputData)
{
    PDEBUG("SubchannelSource::process(outputData: %p, outputSize: %zu)\n",
            outputData, outputData->getLength());

    if (d_buffer.getLength() != d_framesize) {
        throw std::runtime_error(
                "ERROR: Subchannel::process: d_buffer != d_framesize: " +
                std::to_string(d_buffer.getLength()) + " != " +
                std::to_string(d_framesize));
    }
    *outputData = d_buffer;

    return outputData->getLength();
}
