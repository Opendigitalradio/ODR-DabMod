/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SubchannelSource.h"
#include "PcDebug.h"

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


const std::vector<PuncturingRule*>& SubchannelSource::get_rules()
{
    return d_puncturing_rules;
}


SubchannelSource::SubchannelSource(eti_STC &stc) :
    ModInput(ModFormat(0), ModFormat(stc.getSTL() * 8)),
    d_start_address(stc.getStartAddress()),
    d_framesize(stc.getSTL() * 8),
    d_protection(stc.TPL)
{
    PDEBUG("SubchannelSource::SubchannelSource(...) @ %p\n", this);
//    PDEBUG("  Start address: %i\n", d_start_address);
//    PDEBUG("  Framesize: %i\n", d_framesize);
//    PDEBUG("  Protection: %i\n", d_protection);
    if (protectionForm()) {
        if (protectionOption() == 0) {
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((6 * bitrate() / 8) - 3) * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P23));
                break;
            case 2:
                if (bitrate() == 8) {
                    d_puncturing_rules.push_back(new PuncturingRule(
                                5 * 16, P13));
                    d_puncturing_rules.push_back(new PuncturingRule(
                                1 * 16, P12));
                } else {
                    d_puncturing_rules.push_back(new PuncturingRule(
                                ((2 * bitrate() / 8) - 3) * 16, P14));
                    d_puncturing_rules.push_back(new PuncturingRule(
                                ((4 * bitrate() / 8) + 3) * 16, P13));
                }
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((6 * bitrate() / 8) - 3) * 16, P8));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P7));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((4 * bitrate() / 8) - 3) * 16, P3));
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((2 * bitrate() / 8) + 3) * 16, P2));
                break;
            default:
                fprintf(stderr,
                        "Protection form(%zu), option(%zu) and level(%zu)\n",
                        protectionForm(), protectionOption(), protectionLevel());
                fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", stc.TPL, stc.TPL);
                throw std::runtime_error("SubchannelSource::SubchannelSource "
                        "unknown protection level!");
            }
        } else if (protectionOption() == 1) {
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((24 * bitrate() / 32) - 3) * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P9));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((24 * bitrate() / 32) - 3) * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P5));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((24 * bitrate() / 32) - 3) * 16, P4));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P3));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(
                            ((24 * bitrate() / 32) - 3) * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(
                            3 * 16, P1));
                break;
            default:
                fprintf(stderr,
                        "Protection form(%zu), option(%zu) and level(%zu)\n",
                        protectionForm(), protectionOption(), protectionLevel());
                fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", stc.TPL, stc.TPL);
                throw std::runtime_error("SubchannelSource::SubchannelSource "
                        "unknown protection level!");
            }
        } else {
            fprintf(stderr,
                    "Protection form(%zu), option(%zu) and level(%zu)\n",
                    protectionForm(), protectionOption(), protectionLevel());
            fprintf(stderr, "Subchannel TPL: 0x%x (%u)\n", stc.TPL, stc.TPL);
            throw std::runtime_error("SubchannelSource::SubchannelSource "
                    "unknown protection option!");
        }
    } else {
        bool error = false;
        switch (bitrate()) {
        case 32:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(5  * 16, P17));
                d_puncturing_rules.push_back(new PuncturingRule(13 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P17));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P22));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(14 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P15));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(14 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P8 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(18 * 16, P5 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P3 ));
                d_puncturing_rules.push_back(new PuncturingRule(17 * 16, P2 ));
                break;
            default:
                error = true;
            }
            break;
        case 48:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(5  * 16, P18));
                d_puncturing_rules.push_back(new PuncturingRule(25 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P18));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P14));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P15));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P15));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(4  * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P3 ));
                break;
            default:
                error = true;
            }
            break;
        case 56:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P23));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(23 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P7 ));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(23 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P5 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(23 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P3 ));
                break;
            default:
                error = true;
            }
            break;
        case 64:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P18));
                d_puncturing_rules.push_back(new PuncturingRule(28 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P18));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P23));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(29 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(27 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(9  * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(33 * 16, P5 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(9  * 16, P3 ));
                d_puncturing_rules.push_back(new PuncturingRule(31 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(2  * 16, P3 ));
                break;
            default:
                error = true;
            }
            break;
        case 80:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P17));
                d_puncturing_rules.push_back(new PuncturingRule(41 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P18));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P23));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(41 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(40 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P7 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(41 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P3 ));
                d_puncturing_rules.push_back(new PuncturingRule(41 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P3 ));
                break;
            default:
                error = true;
            }
            break;
        case 96:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(13 * 16, P18));
                d_puncturing_rules.push_back(new PuncturingRule(50 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P19));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P22));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(53 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P12));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(6  * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(51 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(7  * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(10 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(52 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(7  * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(9  * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(53 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P4 ));
                break;
            default:
                error = true;
            }
            break;
        case 112:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P23));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(49 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P14));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(23 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(47 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(49 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P8 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(14 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(17 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(50 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P5 ));
                break;
            default:
                error = true;
            }
            break;
        case 128:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(20 * 16, P17));
                d_puncturing_rules.push_back(new PuncturingRule(62 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P19));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P22));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(61 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P14));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(60 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(61 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P7 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(19 * 16, P3 ));
                d_puncturing_rules.push_back(new PuncturingRule(62 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P4 ));
                break;
            default:
                error = true;
            }
            break;
        case 160:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P18));
                d_puncturing_rules.push_back(new PuncturingRule(84 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P19));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P22));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(85 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P8 ));
                d_puncturing_rules.push_back(new PuncturingRule(82 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P11));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P11));
                d_puncturing_rules.push_back(new PuncturingRule(23 * 16, P6 ));
                d_puncturing_rules.push_back(new PuncturingRule(83 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9 ));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P5 ));
                d_puncturing_rules.push_back(new PuncturingRule(19 * 16, P4 ));
                d_puncturing_rules.push_back(new PuncturingRule(87 * 16, P2 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P4 ));
                break;
            default:
                error = true;
            }
            break;
        case 192:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(21 * 16, P20));
                d_puncturing_rules.push_back(new PuncturingRule(109 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P24));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P22));
                d_puncturing_rules.push_back(new PuncturingRule(20 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(110 * 16, P9));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(106 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P11));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(108 * 16, P4));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(20 * 16, P4));
                d_puncturing_rules.push_back(new PuncturingRule(110 * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P5));
                break;
            default:
                error = true;
            }
            break;
        case 224:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P20));
                d_puncturing_rules.push_back(new PuncturingRule(130 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P20));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(132 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P15));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(20 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(134 * 16, P7));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P9));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P8));
                d_puncturing_rules.push_back(new PuncturingRule(127 * 16, P4));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P11));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P8));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(131 * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6));
                break;
            default:
                error = true;
            }
            break;
        case 256:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P19));
                d_puncturing_rules.push_back(new PuncturingRule(152 * 16, P14));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P18));
                break;
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(22 * 16, P14));
                d_puncturing_rules.push_back(new PuncturingRule(156 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P13));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(27 * 16, P10));
                d_puncturing_rules.push_back(new PuncturingRule(151 * 16, P7));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P12));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P9));
                d_puncturing_rules.push_back(new PuncturingRule(154 * 16, P5));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P5));
                d_puncturing_rules.push_back(new PuncturingRule(154 * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P5));
                break;
            default:
                error = true;
            }
            break;
        case 320:
            switch (protectionLevel()) {
            case 2:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P17));
                d_puncturing_rules.push_back(new PuncturingRule(200 * 16, P9 ));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P17));
                break;
            case 4:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P13));
                d_puncturing_rules.push_back(new PuncturingRule(25 * 16, P9));
                d_puncturing_rules.push_back(new PuncturingRule(201 * 16, P5));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P8));
                d_puncturing_rules.push_back(new PuncturingRule(26 * 16, P5));
                d_puncturing_rules.push_back(new PuncturingRule(200 * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P6));
                break;
            default:
                error = true;
            }
            break;
        case 384:
            switch (protectionLevel()) {
            case 1:
                d_puncturing_rules.push_back(new PuncturingRule(12 * 16, P24));
                d_puncturing_rules.push_back(new PuncturingRule(28 * 16, P20));
                d_puncturing_rules.push_back(new PuncturingRule(245 * 16, P14));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P23));
                break;
            case 3:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P16));
                d_puncturing_rules.push_back(new PuncturingRule(24 * 16, P9));
                d_puncturing_rules.push_back(new PuncturingRule(250 * 16, P7));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P10));
                break;
            case 5:
                d_puncturing_rules.push_back(new PuncturingRule(11 * 16, P8));
                d_puncturing_rules.push_back(new PuncturingRule(27 * 16, P6));
                d_puncturing_rules.push_back(new PuncturingRule(247 * 16, P2));
                d_puncturing_rules.push_back(new PuncturingRule(3  * 16, P7));
                break;
            default:
                error = true;
            }
            break;
        default:
            error = true;
        }
        if (error) {
            fprintf(stderr, " Protection: UEP-%zu @ %zukb/s\n",
                    protectionLevel(), bitrate());
            throw std::runtime_error("SubchannelSource::SubchannelSource "
                    "UEP puncturing rules does not exist!");
        }
    }
}


SubchannelSource::~SubchannelSource()
{
    PDEBUG("SubchannelSource::~SubchannelSource() @ %p\n", this);
    for (unsigned i = 0; i < d_puncturing_rules.size(); ++i) {
//        PDEBUG(" Deleting rules @ %p\n", d_puncturing_rules[i]);
        delete d_puncturing_rules[i];
    }
}


size_t SubchannelSource::startAddress()
{
    return d_start_address;
}


size_t SubchannelSource::framesize()
{
    return d_framesize;
}


size_t SubchannelSource::framesizeCu()
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


size_t SubchannelSource::bitrate()
{
    return d_framesize / 3;
}
    
    
size_t SubchannelSource::protection()
{
    return d_protection;
}


size_t SubchannelSource::protectionForm()
{
    return (d_protection >> 5) & 1;
}


size_t SubchannelSource::protectionLevel()
{
    if (protectionForm()) { // Long form
        return (d_protection & 0x3) + 1;
    }   // Short form
    return (d_protection & 0x7) + 1;
}


size_t SubchannelSource::protectionOption()
{
    if (protectionForm()) { // Long form
        return (d_protection >> 2) & 0x7;
    }   // Short form
    return 0;
}


int SubchannelSource::process(Buffer* inputData, Buffer* outputData)
{
    PDEBUG("SubchannelSource::process"
            "(inputData: %p, outputData: %p)\n",
            inputData, outputData);
    
    if (inputData != NULL && inputData->getLength()) {
        PDEBUG(" Input, storing data\n");
        if (inputData->getLength() != d_framesize) {
            PDEBUG("ERROR: Subchannel::process.inputSize != d_framesize\n");
            exit(-1);
        }
        d_buffer = *inputData;
        return inputData->getLength();
    }
    PDEBUG(" Output, retriving data\n");
    
    return read(outputData);
}


int SubchannelSource::read(Buffer* outputData)
{
    PDEBUG("SubchannelSource::read(outputData: %p, outputSize: %zu)\n",
            outputData, outputData->getLength());
    
    if (d_buffer.getLength() != d_framesize) {
        PDEBUG("ERROR: Subchannel::read.outputSize != d_framesize\n");
        exit(-1);
    }
    *outputData = d_buffer;
    
    return outputData->getLength();
}
