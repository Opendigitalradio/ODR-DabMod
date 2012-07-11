/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)
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


#endif // ETI_H
