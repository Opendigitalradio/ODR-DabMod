/*
   Copyright (C) 2011
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
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

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#if HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
#endif



Buffer::Buffer(size_t len, const void *data)
{
    PDEBUG("Buffer::Buffer(%zu, %p)\n", len, data);

    this->len = 0;
    this->size = 0;
    this->data = NULL;
    setData(data, len);
}


Buffer::~Buffer()
{
    PDEBUG("Buffer::~Buffer() len=%zu, data=%p\n", len, data);
    free(data);
}


Buffer &Buffer::operator=(const Buffer &copy)
{
    setData(copy.data, copy.len);
    return *this;
}


Buffer &Buffer::operator+=(const Buffer &copy)
{
    appendData(copy.data, copy.len);
    return *this;
}


void Buffer::setLength(size_t len)
{
    if (len > size) {
        void *tmp = data;

        /* Align to 32-byte boundary for AVX. */
        data = memalign(32, len);

        if (tmp != NULL) {
            memcpy(data, tmp, this->len);
            free(tmp);
        }
        size = len;
    }
    this->len = len;
}


void Buffer::setData(const void *data, size_t len)
{
    setLength(0);
    appendData(data, len);
}


void Buffer::appendData(const void *data, size_t len)
{
    size_t offset = this->len;
    setLength(this->len + len);
    if (data != NULL) {
        memcpy((char*)this->data + offset, data, len);
    }
}

