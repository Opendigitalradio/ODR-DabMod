/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014
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

#ifndef ETI_READER_H
#define ETI_READER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "Eti.h"
#include "Log.h"
#include "FicSource.h"
#include "SubchannelSource.h"
#include "TimestampDecoder.h"

#include <vector>
#include <stdint.h>
#include <sys/types.h>


class EtiReader
{
public:
    EtiReader(struct modulator_offset_config& modconf, Logger& logger);
    virtual ~EtiReader();
    EtiReader(const EtiReader&);
    EtiReader& operator=(const EtiReader&);

    FicSource* getFic();
    unsigned getMode();
    unsigned getFp();
    const std::vector<SubchannelSource*>& getSubchannels();
    int process(Buffer* dataIn);

    void calculateTimestamp(struct frame_timestamp& ts)
    {
        myTimestampDecoder.calculateTimestamp(ts);
    }

    /* Returns true if we have valid time stamps in the ETI*/
    bool sourceContainsTimestamp();

protected:
    /* Main program logger */
    Logger& myLogger;

    /* Transform the ETI TIST to a PPS offset in ms */
    double getPPSOffset();

    void sync();
    int state;
    uint32_t nb_frames;
    uint16_t framesize;
    eti_SYNC eti_sync;
    eti_FC eti_fc;
    std::vector<eti_STC> eti_stc;
    eti_EOH eti_eoh;
    eti_EOF eti_eof;
    eti_TIST eti_tist;
    FicSource* myFicSource;
    std::vector<SubchannelSource*> mySources;
    TimestampDecoder myTimestampDecoder;
    
private:
    size_t myCurrentFrame;
    bool time_ext_enabled;
    unsigned long timestamp_seconds;
};


#endif // ETI_READER_H
