/*
   Copyright (C) 2011
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2018
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

#include "Buffer.h"
#include "PcDebug.h"

#include <string>
#include <stdlib.h>
#include <string.h>

Buffer::Buffer(size_t len, const void *data)
{
    PDEBUG("Buffer::Buffer(%zu, %p)\n", len, data);

    m_len = 0;
    m_capacity = 0;
    m_data = nullptr;
    setData(data, len);
}

Buffer::Buffer(const Buffer& other)
{
    setData(other.m_data, other.m_len);
}

Buffer::Buffer(Buffer&& other)
{
    m_len = other.m_len;
    m_capacity = other.m_capacity;
    m_data = other.m_data;

    other.m_len = 0;
    other.m_capacity = 0;
    other.m_data = nullptr;
}

Buffer::Buffer(const std::vector<uint8_t>& vec)
{
    PDEBUG("Buffer::Buffer(vector [%zu])\n", vec.size());

    m_len = 0;
    m_capacity = 0;
    m_data = nullptr;
    setData(vec.data(), vec.size());
}


Buffer::~Buffer()
{
    PDEBUG("Buffer::~Buffer() len=%zu, data=%p\n", m_len, m_data);
    if (m_data) {
        free(m_data);
    }
}

void Buffer::swap(Buffer& other)
{
    std::swap(m_len, other.m_len);
    std::swap(m_capacity, other.m_capacity);
    std::swap(m_data, other.m_data);
}

Buffer& Buffer::operator=(const Buffer& other)
{
    if (&other != this) {
        setData(other.m_data, other.m_len);
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& other)
{
    if (&other != this) {
        m_len = other.m_len;
        m_capacity = other.m_capacity;
        if (m_data != nullptr) {
            free(m_data);
        }
        m_data = other.m_data;

        other.m_len = 0;
        other.m_capacity = 0;
        other.m_data = nullptr;
    }

    return *this;
}

Buffer& Buffer::operator=(const std::vector<uint8_t>& buf)
{
    setData(buf.data(), buf.size());
    return *this;
}

Buffer& Buffer::operator+=(const Buffer& other)
{
    appendData(other.m_data, other.m_len);
    return *this;
}


void Buffer::setLength(size_t len)
{
    if (len > m_capacity) {
        void *tmp = m_data;

        /* Align to 32-byte boundary for AVX. */
        const int ret = posix_memalign(&m_data, 32, len);
        if (ret != 0) {
            throw std::runtime_error("memory allocation failed: " +
                    std::to_string(ret));
        }

        if (tmp != nullptr) {
            memcpy(m_data, tmp, m_len);
            free(tmp);
        }
        m_capacity = len;
    }
    m_len = len;
}


void Buffer::setData(const void *data, size_t len)
{
    setLength(0);
    appendData(data, len);
}

uint8_t Buffer::operator[](size_t i) const
{
    if (i >= m_len) {
        throw std::out_of_range("index out of range");
    }
    return reinterpret_cast<uint8_t*>(m_data)[i];
}

void Buffer::appendData(const void *data, size_t len)
{
    size_t offset = m_len;
    setLength(m_len + len);

    if (data != nullptr) {
        memcpy((char*)m_data + offset, data, len);
    }
}

void swap(Buffer& buf1, Buffer& buf2)
{
    buf1.swap(buf2);
}

