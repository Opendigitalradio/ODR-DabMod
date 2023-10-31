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

#pragma once
#include <vector>
#include <stdexcept>
#include <string>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstring>

class CharsetTools {
    private:
        static const char* no_char;
        static const char* ebu_values_0x00_to_0x1F[];
        static const char* ebu_values_0x7B_to_0xFF[];
        static std::string ConvertCharEBUToUTF8(const uint8_t value);
    public:
        static std::string ConvertTextToUTF8(const uint8_t *data, size_t len, int charset, std::string* charset_name);
};

typedef std::vector<std::string> string_vector_t;

// --- StringTools -----------------------------------------------------------------
class StringTools {
private:
	static size_t UTF8CharsLen(const std::string &s, size_t chars);
public:
	static size_t UTF8Len(const std::string &s);
	static std::string UTF8Substr(const std::string &s, size_t pos, size_t count);
};
