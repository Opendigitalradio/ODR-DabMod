/*
   Copyright (C) 2016
   Matthias P. Braendli, matthias.braendli@mpb.li

   http://opendigitalradio.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once
#include <stdint.h>

namespace EdiDecoder {

template<class T>
uint16_t read_16b(T buf)
{
    uint16_t value = 0;
    value = (uint16_t)(buf[0]) << 8;
    value |= (uint16_t)(buf[1]);
    return value;
}

template<class T>
uint32_t read_24b(T buf)
{
    uint32_t value = 0;
    value = (uint32_t)(buf[0]) << 16;
    value |= (uint32_t)(buf[1]) << 8;
    value |= (uint32_t)(buf[2]);
    return value;
}

template<class T>
uint32_t read_32b(T buf)
{
    uint32_t value = 0;
    value = (uint32_t)(buf[0]) << 24;
    value |= (uint32_t)(buf[1]) << 16;
    value |= (uint32_t)(buf[2]) << 8;
    value |= (uint32_t)(buf[3]);
    return value;
}

inline uint32_t unpack1bit(uint8_t byte, int bitpos)
{
    return (byte & 1 << (7-bitpos)) > (7-bitpos);
}

}
