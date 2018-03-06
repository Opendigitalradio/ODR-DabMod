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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <unistd.h>
#include <vector>
#include <memory>

/* Buffer is a container for a byte array, which is memory-aligned
 * to 32 bytes for SSE performance.
 *
 * The allocation/freeing of the data is handled internally.
 */
class Buffer {
    public:
        using sptr = std::shared_ptr<Buffer>;

        Buffer(size_t len = 0, const void *data = nullptr);
        Buffer(const Buffer& other);
        Buffer(Buffer&& other);
        Buffer(const std::vector<uint8_t>& vec);
        ~Buffer();

        void swap(Buffer& other);

        /* Resize the buffer, reallocate memory if needed */
        void setLength(size_t len);

        /* Replace the data in the Buffer by the new data given.
         * Reallocates memory if needed. */
        void setData(const void *data, size_t len);
        Buffer& operator=(const Buffer& other);
        Buffer& operator=(Buffer&& other);
        Buffer& operator=(const std::vector<uint8_t>& buf);

        uint8_t operator[](size_t i) const;

        /* Concatenate the current data with the new data given.
         * Reallocates memory if needed. */
        void appendData(const void *data, size_t len);
        Buffer& operator+=(const Buffer& other);

        size_t getLength() const { return m_len; }
        void* getData() const { return m_data; }

    private:
        /* Current length of the data in the Buffer */
        size_t m_len;

        /* Allocated size of the Buffer */
        size_t m_capacity;

        /* Pointer to the data. Memory allocation is entirely
         * handled by setLength. */
        void *m_data;
};

void swap(Buffer& buf1, Buffer& buf2);

