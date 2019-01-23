/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

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

#include <cstdint>
#include <memory>
#include <string>
#include <math.h>
#include <stdio.h>
#include "RemoteControl.h"

struct frame_timestamp
{
    // Which frame count does this timestamp apply to
    int32_t fct;
    uint8_t fp; // Frame Phase

    uint32_t timestamp_sec;
    uint32_t timestamp_pps; // In units of 1/16384000 s
    bool timestamp_valid = false;
    bool timestamp_refresh;

    frame_timestamp& operator+=(const double& diff);

    const frame_timestamp operator+(const double diff) const {
        frame_timestamp ts = *this;
        ts += diff;
        return ts;
    }

    double pps_offset() const {
        return timestamp_pps / 16384000.0;
    }

    double get_real_secs() const {
        double t = timestamp_sec;
        t += pps_offset();
        return t;
    }

    long long int get_ns() const {
        long long int ns = timestamp_sec * 1000000000ull;
        ns += llrint((double)timestamp_pps / 0.016384);
        return ns;
    }

    void set_ns(long long int time_ns) {
        timestamp_sec = time_ns / 1000000000ull;
        const long long int subsecond = time_ns % 1000000000ull;
        timestamp_pps = lrint(subsecond * 16384000.0);
    }

    void print(const char* t) const {
        etiLog.log(debug,
                "%s <frame_timestamp(%s, %d, %.9f, %d)>\n",
                t, this->timestamp_valid ? "valid" : "invalid",
                 this->timestamp_sec, pps_offset(),
                 this->fct);
    }
};

/* This module decodes MNSC time information from an ETI source and
 * EDI time information*/
class TimestampDecoder : public RemoteControllable
{
    public:
        /* offset_s: The modulator adds this offset to the TIST to define time of
         * frame transmission
         */
        TimestampDecoder(double& offset_s);

        std::shared_ptr<frame_timestamp> getTimestamp(void);

        /* Update timestamp data from ETI */
        void updateTimestampEti(
                uint8_t framephase,
                uint16_t mnsc,
                uint32_t pps, // In units of 1/16384000 s
                int32_t fct);

        /* Update timestamp data from EDI */
        void updateTimestampEdi(
                uint32_t seconds_utc,
                uint32_t pps, // In units of 1/16384000 s
                int32_t fct,
                uint8_t framephase);

        /*********** REMOTE CONTROL ***************/

        /* Base function to set parameters. */
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(
                const std::string& parameter) const;

        const char* name() { return "TS"; }


    protected:
        /* Push a new MNSC field into the decoder */
        void pushMNSCData(uint8_t framephase, uint16_t mnsc);

        /* Each frame contains the TIST field with the PPS offset.
         * For each frame, this function must be called to update
         * the timestamp.
         *
         * pps is in units of 1/16384000 s
         *
         * This function also takes care of updating the second when
         * the pps rolls over.
         */
        void updateTimestampPPS(uint32_t pps);

        /* Update the timestamp when a full set of MNSC data is
         * known. This function can be called at most every four
         * frames when the data is transferred using the MNSC.
         */
        void updateTimestampSeconds(uint32_t secs);

        struct tm temp_time;
        uint32_t time_secs = 0;
        int32_t latestFCT = 0;
        uint32_t latestFP = 0;
        uint32_t time_pps = 0;
        double& timestamp_offset;
        int inhibit_second_update = 0;
        bool offset_changed = false;

        uint32_t time_secs_of_frame0 = 0;
        uint32_t time_pps_of_frame0 = 0;

        /* When the type or identifier don't match, the decoder must
         * be disabled
         */
        bool enableDecode = false;

        /* Disable timstamps until full time has been received */
        bool full_timestamp_received = false;
};

