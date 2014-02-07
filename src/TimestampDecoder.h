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

#ifndef TIMESTAMP_DECODER_H
#define TIMESTAMP_DECODER_H

#include <queue>
#include <string>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include "Eti.h"
#include "Log.h"

struct modulator_offset_config
{
    bool use_offset_fixed;
    double offset_fixed;
    /* These two fields are used when the modulator is run with a fixed offset */

    bool use_offset_file;
    std::string offset_filename;
    /* These two fields are used when the modulator reads the offset from a file */

    unsigned delay_calculation_pipeline_stages;
    /* Specifies by how many stages the timestamp must be delayed.
     * (e.g. The FIRFilter is pipelined, therefore we must increase 
     * delay_calculation_pipeline_stages by one if the filter is used
     */
};

struct frame_timestamp
{
    uint32_t timestamp_sec;
    double timestamp_pps_offset;
    bool timestamp_valid;
    bool timestamp_refresh;

    struct frame_timestamp operator=(const struct frame_timestamp &rhs)
    {
        if (this != &rhs) {
            this->timestamp_sec = rhs.timestamp_sec;
            this->timestamp_pps_offset = rhs.timestamp_pps_offset;
            this->timestamp_valid = rhs.timestamp_valid;
            this->timestamp_refresh = rhs.timestamp_refresh;
        }

        return *this;
    }

    struct frame_timestamp& operator+=(const double& diff)
    {
        double offset_pps, offset_secs;
        offset_pps = modf(diff, &offset_secs);

        this->timestamp_sec += lrintf(offset_secs);
        this->timestamp_pps_offset += offset_pps;

        while (this->timestamp_pps_offset > 1)
        {
            this->timestamp_pps_offset -= 1.0;
            this->timestamp_sec += 1;
        };
        return *this;
    }

    const struct frame_timestamp operator+(const double diff)
    {
        struct frame_timestamp ts = *this;
        ts += diff;
        return ts;
    }

    void print(const char* t)
    {
        fprintf(stderr,
                "%s <struct frame_timestamp(%s, %d, %.9f)>\n", 
                t, this->timestamp_valid ? "valid" : "invalid",
                 this->timestamp_sec, this->timestamp_pps_offset);
    }
};

/* This module decodes MNSC time information */
class TimestampDecoder
{
    public: 
        TimestampDecoder(
                struct modulator_offset_config& config,
                Logger& logger):
            myLogger(logger), modconfig(config)
        {
            inhibit_second_update = 0;
            time_pps = 0.0;
            time_secs = 0;
            enableDecode = false;
            full_timestamp_received_mnsc = false;
            gmtime_r(0, &temp_time);
            offset_changed = false;

            myLogger.level(info) << "Setting up timestamp decoder with " << 
                (modconfig.use_offset_fixed ? "fixed" : 
                (modconfig.use_offset_file ? "dynamic" : "none")) <<
                " offset";

        };

        /* Calculate the timestamp for the current frame. */
        void calculateTimestamp(struct frame_timestamp& ts);

        /* Update timestamp data from data in ETI */
        void updateTimestampEti(int framephase, uint16_t mnsc, double pps);

        /* Update the modulator timestamp offset according to the modconf
         */
        bool updateModulatorOffset();

    protected:
        /* Main program logger */
        Logger& myLogger;

        /* Push a new MNSC field into the decoder */
        void pushMNSCData(int framephase, uint16_t mnsc);

        /* Each frame contains the TIST field with the PPS offset.
         * For each frame, this function must be called to update
         * the timestamp
         */
        void updateTimestampPPS(double pps);
        
        /* Update the timestamp when a full set of MNSC data is
         * known. This function can be called at most every four
         * frames when the data is transferred using the MNSC.
         */
        void updateTimestampSeconds(uint32_t secs);

        struct tm temp_time;
        uint32_t time_secs;
        double time_pps;
        double timestamp_offset;
        int inhibit_second_update;
        bool offset_changed;

        /* configuration for the offset management */
        struct modulator_offset_config& modconfig;

        /* When the type or identifier don't match, the decoder must
         * be disabled
         */
        bool enableDecode;

        /* Disable timstamps until full time has been received in mnsc */
        bool full_timestamp_received_mnsc;

        /* when pipelining, we must shift the calculated timestamps
         * through this queue. Otherwise, it would not be possible to
         * synchronise two modulators if only one uses (for instance) the
         * FIRFilter (1 stage pipeline)
         */
        std::queue<struct frame_timestamp*> queue_timestamps;

};

#endif
