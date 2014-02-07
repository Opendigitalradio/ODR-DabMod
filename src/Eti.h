/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Includes modifications for which no copyright is claimed
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
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

#ifndef ETI_H
#define ETI_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _WIN32
#	include <winsock2.h>	// For types...
typedef WORD uint16_t;
typedef DWORD32 uint32_t;

#   define PACKED
#   pragma pack(push, 1)
#else
#   include <stdint.h>

#   define PACKED __attribute__ ((packed))
#endif

#include <time.h>

//definitions des structures des champs du ETI(NI, G703)


struct eti_SYNC {
    uint32_t ERR:8;
    uint32_t FSYNC:24;
} PACKED;


struct eti_FC {
    uint32_t FCT:8;
    uint32_t NST:7;
    uint32_t FICF:1;
    uint32_t FL_high:3;
    uint32_t MID:2;
    uint32_t FP:3;
    uint32_t FL_low:8;
    uint16_t getFrameLength();
    void setFrameLength(uint16_t length);
} PACKED;


struct eti_STC {
    uint32_t startAddress_high:2;
    uint32_t SCID:6;
    uint32_t startAddress_low:8;
    uint32_t STL_high:2;
    uint32_t TPL:6;
    uint32_t STL_low:8;
    void setSTL(uint16_t length);
    uint16_t getSTL();
    void setStartAddress(uint16_t address);
    uint16_t getStartAddress();
} PACKED;


struct eti_EOH {
    uint16_t MNSC;
    uint16_t CRC;
} PACKED;


struct eti_EOF {
    uint16_t CRC;
    uint16_t RFU;
} PACKED;


struct eti_TIST {
    uint32_t TIST;
} PACKED;

struct eti_MNSC_TIME_0 {
    uint32_t type:4;
    uint32_t identifier:4;
    uint32_t rfa:8;
} PACKED;

struct eti_MNSC_TIME_1 {
    uint32_t second_unit:4;
    uint32_t second_tens:3;
    uint32_t accuracy:1;
    
    uint32_t minute_unit:4;
    uint32_t minute_tens:3;
    uint32_t sync_to_frame:1;
} PACKED;

struct eti_MNSC_TIME_2 {
    uint32_t hour_unit:4;
    uint32_t hour_tens:4;
    
    uint32_t day_unit:4;
    uint32_t day_tens:4;
} PACKED;

struct eti_MNSC_TIME_3 {
    uint32_t month_unit:4;
    uint32_t month_tens:4;
    
    uint32_t year_unit:4;
    uint32_t year_tens:4;
} PACKED;

struct eti_extension_TIME {
    uint32_t TIME_SECONDS;
} PACKED;


#endif // ETI_H
