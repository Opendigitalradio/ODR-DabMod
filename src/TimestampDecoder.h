/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014, 2015
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

#ifndef TIMESTAMP_DECODER_H
#define TIMESTAMP_DECODER_H

#include <queue>
#include <memory>
#include <string>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include "Eti.h"
#include "Log.h"
#include "RemoteControl.h"

struct frame_timestamp
{
    // Which frame count does this timestamp apply to
    int32_t fct;

    uint32_t timestamp_sec;
    uint32_t timestamp_pps; // In units of 1/16384000 s
    bool timestamp_valid;
    bool timestamp_refresh;

    struct frame_timestamp operator=(const struct frame_timestamp &rhs)
    {
        if (this != &rhs) {
            this->timestamp_sec = rhs.timestamp_sec;
            this->timestamp_pps = rhs.timestamp_pps;
            this->timestamp_valid = rhs.timestamp_valid;
            this->timestamp_refresh = rhs.timestamp_refresh;
            this->fct = rhs.fct;
        }

        return *this;
    }

    struct frame_timestamp& operator+=(const double& diff)
    {
        double offset_pps, offset_secs;
        offset_pps = modf(diff, &offset_secs);

        this->timestamp_sec += lrintf(offset_secs);
        this->timestamp_pps += lrintf(offset_pps * 16384000.0);

        while (this->timestamp_pps > 16384000)
        {
            this->timestamp_pps -= 16384000;
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

    double pps_offset() const {
        return timestamp_pps / 16384000.0;
    }

    void print(const char* t)
    {
        fprintf(stderr,
                "%s <struct frame_timestamp(%s, %d, %.9f, %d)>\n",
                t, this->timestamp_valid ? "valid" : "invalid",
                 this->timestamp_sec, pps_offset(),
                 this->fct);
    }
};

/* This module decodes MNSC time information */
class TimestampDecoder : public RemoteControllable
{
    public:
        TimestampDecoder(
                /* The modulator adds this offset to the TIST to define time of
                 * frame transmission
                 */
                double& offset_s,

                /* Specifies by how many stages the timestamp must be delayed.
                 * (e.g. The FIRFilter is pipelined, therefore we must increase
                 * tist_delay_stages by one if the filter is used
                 */
                unsigned tist_delay_stages) :
            RemoteControllable("tist"),
            timestamp_offset(offset_s)
        {
            m_tist_delay_stages = tist_delay_stages;
            inhibit_second_update = 0;
            time_pps = 0.0;
            time_secs = 0;
            latestFCT = 0;
            enableDecode = false;
            full_timestamp_received_mnsc = false;
            gmtime_r(0, &temp_time);
            offset_changed = false;

            RC_ADD_PARAMETER(offset, "TIST offset [s]");

            etiLog.level(info) << "Setting up timestamp decoder with " <<
                timestamp_offset << " offset";

        };

        /* Calculate the timestamp for the current frame. */
        void calculateTimestamp(struct frame_timestamp& ts);

        /* Update timestamp data from data in ETI */
        void updateTimestampEti(
                int framephase,
                uint16_t mnsc,
                uint32_t pps, // In units of 1/16384000 s
                int32_t fct);

        /*********** REMOTE CONTROL ***************/
        /* virtual void enrol_at(BaseRemoteController& controller)
         * is inherited
         */

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const;

        const char* name() { return "TS"; }


    protected:
        /* Push a new MNSC field into the decoder */
        void pushMNSCData(int framephase, uint16_t mnsc);

        /* Each frame contains the TIST field with the PPS offset.
         * For each frame, this function must be called to update
         * the timestamp.
         *
         * pps is in units of 1/16384000 s
         */
        void updateTimestampPPS(uint32_t pps);

        /* Update the timestamp when a full set of MNSC data is
         * known. This function can be called at most every four
         * frames when the data is transferred using the MNSC.
         */
        void updateTimestampSeconds(uint32_t secs);

        struct tm temp_time;
        uint32_t time_secs;
        int32_t latestFCT;
        uint32_t time_pps;
        double& timestamp_offset;
        unsigned m_tist_delay_stages;
        int inhibit_second_update;
        bool offset_changed;

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
        std::queue<std::shared_ptr<struct frame_timestamp> > queue_timestamps;

};

#endif

