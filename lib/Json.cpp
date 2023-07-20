/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org
 */
/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>

#include "Json.h"

namespace json {
    static std::string escape_json(const std::string &s) {
        std::ostringstream o;
        for (auto c = s.cbegin(); c != s.cend(); c++) {
            switch (*c) {
                case '"': o << "\\\""; break;
                case '\\': o << "\\\\"; break;
                case '\b': o << "\\b"; break;
                case '\f': o << "\\f"; break;
                case '\n': o << "\\n"; break;
                case '\r': o << "\\r"; break;
                case '\t': o << "\\t"; break;
                default:
                           if ('\x00' <= *c && *c <= '\x1f') {
                               o << "\\u"
                                   << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(*c);
                           } else {
                               o << *c;
                           }
            }
        }
        return o.str();
    }

    std::string map_to_json(const map_t& values) {
        std::ostringstream ss;
        ss << "{ ";
        size_t ix = 0;
        for (const auto& element : values) {
            if (ix > 0) {
                ss << ",";
            }

            ss << "\"" << escape_json(element.first) << "\": ";

            const auto& value = element.second.v;
            if (std::holds_alternative<std::string>(value)) {
                ss << "\"" << escape_json(std::get<std::string>(value)) << "\"";
            }
            else if (std::holds_alternative<double>(value)) {
                ss << std::defaultfloat << std::get<double>(value);
            }
            else if (std::holds_alternative<ssize_t>(value)) {
                ss << std::get<ssize_t>(value);
            }
            else if (std::holds_alternative<size_t>(value)) {
                ss << std::get<size_t>(value);
            }
            else if (std::holds_alternative<bool>(value)) {
                ss << (std::get<bool>(value) ? "true" : "false");
            }
            else if (std::holds_alternative<std::nullopt_t>(value)) {
                ss << "null";
            }
            else if (std::holds_alternative<std::shared_ptr<json::map_t> >(value)) {
                const map_t& v = *std::get<std::shared_ptr<json::map_t> >(value);
                ss << map_to_json(v);
            }
            else {
                throw std::logic_error("variant alternative not handled");
            }

            ix++;
        }
        ss << " }";

        return ss.str();
    }
}
