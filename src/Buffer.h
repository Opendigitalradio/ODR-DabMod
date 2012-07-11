/*
   Copyright (C) 2011
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
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

#ifndef BUFFER_H
#define BUFFER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <unistd.h>


class Buffer {
protected:
    size_t len;
    size_t size;
    void *data;

public:
    Buffer(const Buffer& copy);
    Buffer(size_t len = 0, const void *data = NULL);
    ~Buffer();

    Buffer &operator=(const Buffer &copy);
    Buffer &operator+=(const Buffer &copy);

    void setLength(size_t len);
    void setData(const void *data, size_t len);
    void appendData(const void *data, size_t len);

    size_t getLength();
    void *getData();
};


#endif // BUFFER_H
