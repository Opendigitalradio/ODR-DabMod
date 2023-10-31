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
            ss << value_to_json(element.second);

            ix++;
        }
        ss << " }";

        return ss.str();
    }

    std::string value_to_json(const value_t& value)
    {
        std::ostringstream ss;

        if (std::holds_alternative<std::string>(value.v)) {
            ss << "\"" << escape_json(std::get<std::string>(value.v)) << "\"";
        }
        else if (std::holds_alternative<double>(value.v)) {
            ss << std::fixed << std::get<double>(value.v);
        }
        else if (std::holds_alternative<ssize_t>(value.v)) {
            ss << std::get<ssize_t>(value.v);
        }
        else if (std::holds_alternative<size_t>(value.v)) {
            ss << std::get<size_t>(value.v);
        }
        else if (std::holds_alternative<bool>(value.v)) {
            ss << (std::get<bool>(value.v) ? "true" : "false");
        }
        else if (std::holds_alternative<std::nullopt_t>(value.v)) {
            ss << "null";
        }
        else if (std::holds_alternative<std::vector<json::value_t> >(value.v)) {
            const auto& vec = std::get<std::vector<json::value_t> >(value.v);
            ss << "[ ";
            size_t list_ix = 0;
            for (const auto& list_element : vec) {
                if (list_ix > 0) {
                    ss << ",";
                }
                ss << value_to_json(list_element);
                list_ix++;
            }
            ss << "]";
        }
        else if (std::holds_alternative<std::shared_ptr<json::map_t> >(value.v)) {
            const map_t& v = *std::get<std::shared_ptr<json::map_t> >(value.v);
            ss << map_to_json(v);
        }
        else {
            throw std::logic_error("variant alternative not handled");
        }

        return ss.str();
    }
}
