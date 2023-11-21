/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Most parts of this file are taken from dablin,
   Copyright (C) 2015-2022 Stefan Pöschel

   Copyright (C) 2023
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

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "CharsetTools.h"

// --- CharsetTools -----------------------------------------------------------------
const char* CharsetTools::no_char = "";
const char* CharsetTools::ebu_values_0x00_to_0x1F[] = {
        no_char , "\u0118", "\u012E", "\u0172", "\u0102", "\u0116", "\u010E", "\u0218", "\u021A", "\u010A", no_char , no_char , "\u0120", "\u0139" , "\u017B", "\u0143",
        "\u0105", "\u0119", "\u012F", "\u0173", "\u0103", "\u0117", "\u010F", "\u0219", "\u021B", "\u010B", "\u0147", "\u011A", "\u0121", "\u013A", "\u017C", no_char
};
const char* CharsetTools::ebu_values_0x7B_to_0xFF[] = {
        /* starting some chars earlier than 0x80 -----> */                                                            "\u00AB", "\u016F", "\u00BB", "\u013D", "\u0126",
        "\u00E1", "\u00E0", "\u00E9", "\u00E8", "\u00ED", "\u00EC", "\u00F3", "\u00F2", "\u00FA", "\u00F9", "\u00D1", "\u00C7", "\u015E", "\u00DF", "\u00A1", "\u0178",
        "\u00E2", "\u00E4", "\u00EA", "\u00EB", "\u00EE", "\u00EF", "\u00F4", "\u00F6", "\u00FB", "\u00FC", "\u00F1", "\u00E7", "\u015F", "\u011F", "\u0131", "\u00FF",
        "\u0136", "\u0145", "\u00A9", "\u0122", "\u011E", "\u011B", "\u0148", "\u0151", "\u0150", "\u20AC", "\u00A3", "\u0024", "\u0100", "\u0112", "\u012A", "\u016A",
        "\u0137", "\u0146", "\u013B", "\u0123", "\u013C", "\u0130", "\u0144", "\u0171", "\u0170", "\u00BF", "\u013E", "\u00B0", "\u0101", "\u0113", "\u012B", "\u016B",
        "\u00C1", "\u00C0", "\u00C9", "\u00C8", "\u00CD", "\u00CC", "\u00D3", "\u00D2", "\u00DA", "\u00D9", "\u0158", "\u010C", "\u0160", "\u017D", "\u00D0", "\u013F",
        "\u00C2", "\u00C4", "\u00CA", "\u00CB", "\u00CE", "\u00CF", "\u00D4", "\u00D6", "\u00DB", "\u00DC", "\u0159", "\u010D", "\u0161", "\u017E", "\u0111", "\u0140",
        "\u00C3", "\u00C5", "\u00C6", "\u0152", "\u0177", "\u00DD", "\u00D5", "\u00D8", "\u00DE", "\u014A", "\u0154", "\u0106", "\u015A", "\u0179", "\u0164", "\u00F0",
        "\u00E3", "\u00E5", "\u00E6", "\u0153", "\u0175", "\u00FD", "\u00F5", "\u00F8", "\u00FE", "\u014B", "\u0155", "\u0107", "\u015B", "\u017A", "\u0165", "\u0127"
};

std::string CharsetTools::ConvertCharEBUToUTF8(const uint8_t value) {
    // convert via LUT
    if(value <= 0x1F)
        return ebu_values_0x00_to_0x1F[value];
    if(value >= 0x7B)
        return ebu_values_0x7B_to_0xFF[value - 0x7B];

    // convert by hand (avoiding a LUT with mostly 1:1 mapping)
    switch(value) {
    case 0x24:
        return "\u0142";
    case 0x5C:
        return "\u016E";
    case 0x5E:
        return "\u0141";
    case 0x60:
        return "\u0104";
    }

    // leave untouched
    return std::string((char*) &value, 1);
}


std::string CharsetTools::ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, std::string* charset_name) {
    // remove undesired chars
    std::vector<uint8_t> cleaned_data;
    for(size_t i = 0; i < len; i++) {
        switch(data[i]) {
        case 0x00:  // NULL
        case 0x0A:  // PLB
        case 0x0B:  // EoH
        case 0x1F:  // PWB
            continue;
        default:
            cleaned_data.push_back(data[i]);
        }
    }

    // convert characters
    if(charset == 0b0000) {         // EBU Latin based
        if(charset_name)
            *charset_name = "EBU Latin based";

        std::string result;
        for(const uint8_t& c : cleaned_data)
            result += ConvertCharEBUToUTF8(c);
        return result;
    }

    if(charset == 0b1111) {         // UTF-8
        if(charset_name)
            *charset_name = "UTF-8";

        return std::string((char*) &cleaned_data[0], cleaned_data.size());
    }

    // ignore unsupported charset
    return "";
}


size_t StringTools::UTF8CharsLen(const std::string &s, size_t chars) {
    size_t result;
    for(result = 0; result < s.size(); result++) {
        // if not a continuation byte, handle counter
        if((s[result] & 0xC0) != 0x80) {
            if(chars == 0)
                break;
            chars--;
        }
    }
    return result;
}

size_t StringTools::UTF8Len(const std::string &s) {
    // ignore continuation bytes
    return std::count_if(s.cbegin(), s.cend(), [](const char c){return (c & 0xC0) != 0x80;});
}

std::string StringTools::UTF8Substr(const std::string &s, size_t pos, size_t count) {
    std::string result = s;
    result.erase(0, UTF8CharsLen(result, pos));
    result.erase(UTF8CharsLen(result, count));
    return result;
}
