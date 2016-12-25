/*
   Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
   */
/*
   This file is part of ODR-DabMux.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#ifndef _CRC
#define _CRC

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifndef _WIN32
  #include <stdint.h>
#else
  #include <winsock2.h>	// For types...
  typedef BYTE uint8_t;
  typedef WORD uint16_t;
  typedef DWORD32 uint32_t;
#endif


#ifdef __cplusplus
extern "C" { // }
#endif

void init_crc8tab(uint8_t l_code, uint8_t l_init);
uint8_t crc8(uint8_t l_crc, const void *lp_data, unsigned l_nb);
extern uint8_t crc8tab[];

void init_crc16tab(uint16_t l_code, uint16_t l_init);
uint16_t crc16(uint16_t l_crc, const void *lp_data, unsigned l_nb);
extern uint16_t crc16tab[];

void init_crc32tab(uint32_t l_code, uint32_t l_init);
uint32_t crc32(uint32_t l_crc, const void *lp_data, unsigned l_nb);
extern uint32_t crc32tab[];

#ifdef __cplusplus
}
#endif

#endif //_CRC
