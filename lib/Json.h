/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://www.opendigitalradio.org

   This module adds remote-control capability to some of the dabmux/dabmod modules.
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

#pragma once

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

namespace json {

    using map_t = std::unordered_map<std::string, struct value_t>;

    struct value_t {
        std::variant<
            map_t,
            std::string,
            double,
            size_t,
            ssize_t,
            bool,
            std::nullopt_t> data;

        template<class T>
        value_t operator=(const T& map) {
            value_t v;
            v.data = map;
            return v;
        }

    };

    std::string map_to_json(const map_t& values);
}
