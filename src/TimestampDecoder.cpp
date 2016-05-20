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

#include <queue>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include "PcDebug.h"
#include "TimestampDecoder.h"
#include "Eti.h"
#include "Log.h"

//#define MDEBUG(fmt, args...) fprintf (LOG, "*****" fmt , ## args) 
#define MDEBUG(fmt, args...) PDEBUG(fmt, ## args) 


void TimestampDecoder::calculateTimestamp(struct frame_timestamp& ts)
{
    std::shared_ptr<struct frame_timestamp> ts_queued =
        std::make_shared<struct frame_timestamp>();

    /* Push new timestamp into queue */
    ts_queued->timestamp_valid = full_timestamp_received_mnsc;
    ts_queued->timestamp_sec = time_secs;
    ts_queued->timestamp_pps = time_pps;
    ts_queued->fct = latestFCT;

    ts_queued->timestamp_refresh = offset_changed;
    offset_changed = false;

    MDEBUG("time_secs=%d, time_pps=%f\n", time_secs,
            (double)time_pps / 16384000.0);
    *ts_queued += timestamp_offset;

    queue_timestamps.push(ts_queued);

    /* Here, the queue size is one more than the pipeline delay, because
     * we've just added a new element in the queue.
     *
     * Therefore, use <= and not < for comparison
     */
    if (queue_timestamps.size() <= m_tist_delay_stages) {
        //fprintf(stderr, "* %zu %u ", queue_timestamps.size(), m_tist_delay_stages);
        /* Return invalid timestamp until the queue is full */
        ts.timestamp_valid = false;
        ts.timestamp_sec = 0;
        ts.timestamp_pps = 0;
        ts.timestamp_refresh = false;
        ts.fct = -1;
    }
    else {
        //fprintf(stderr, ". %zu ", queue_timestamps.size());
        /* Return timestamp from queue */
        ts_queued = queue_timestamps.front();
        queue_timestamps.pop();
        /*fprintf(stderr, "ts_queued v:%d, sec:%d, pps:%f, ref:%d\n",
                ts_queued->timestamp_valid,
                ts_queued->timestamp_sec,
                ts_queued->timestamp_pps_offset,
                ts_queued->timestamp_refresh);*/
        ts = *ts_queued;
        /*fprintf(stderr, "ts v:%d, sec:%d, pps:%f, ref:%d\n\n",
                ts.timestamp_valid,
                ts.timestamp_sec,
                ts.timestamp_pps_offset,
                ts.timestamp_refresh);*/
    }

    MDEBUG("Timestamp queue size %zu, delay_calc %u\n",
            queue_timestamps.size(),
            m_tist_delay_stages);

    if (queue_timestamps.size() > m_tist_delay_stages) {
        etiLog.level(error) << "Error: Timestamp queue is too large : size " <<
            queue_timestamps.size() << "! This should not happen !";
    }

    //ts.print("calc2 ");
}

void TimestampDecoder::pushMNSCData(int framephase, uint16_t mnsc)
{
    struct eti_MNSC_TIME_0 *mnsc0;
    struct eti_MNSC_TIME_1 *mnsc1;
    struct eti_MNSC_TIME_2 *mnsc2;
    struct eti_MNSC_TIME_3 *mnsc3;

    switch (framephase)
    {
        case 0:
            mnsc0 = (struct eti_MNSC_TIME_0*)&mnsc;
            enableDecode = (mnsc0->type == 0) &&
                (mnsc0->identifier == 0);
            gmtime_r(0, &temp_time);
            break;

        case 1:
            mnsc1 = (struct eti_MNSC_TIME_1*)&mnsc;
            temp_time.tm_sec = mnsc1->second_tens * 10 + mnsc1->second_unit;
            temp_time.tm_min = mnsc1->minute_tens * 10 + mnsc1->minute_unit;

            if (!mnsc1->sync_to_frame)
            {
                enableDecode = false;
                PDEBUG("TimestampDecoder: MNSC time info is not synchronised to frame\n");
            }

            break;

        case 2:
            mnsc2 = (struct eti_MNSC_TIME_2*)&mnsc;
            temp_time.tm_hour = mnsc2->hour_tens * 10 + mnsc2->hour_unit;
            temp_time.tm_mday = mnsc2->day_tens * 10 + mnsc2->day_unit;
            break;

        case 3:
            mnsc3 = (struct eti_MNSC_TIME_3*)&mnsc;
            temp_time.tm_mon = (mnsc3->month_tens * 10 + mnsc3->month_unit) - 1;
            temp_time.tm_year = (mnsc3->year_tens * 10 + mnsc3->year_unit) + 100;

            if (enableDecode)
            {
                full_timestamp_received_mnsc = true;
                updateTimestampSeconds(mktime(&temp_time));
            }
            break;
    }

    MDEBUG("TimestampDecoder::pushMNSCData(%d, 0x%x)\n", framephase, mnsc);
    MDEBUG("                            -> %s\n", asctime(&temp_time));
    MDEBUG("                            -> %zu\n", mktime(&temp_time));
}

void TimestampDecoder::updateTimestampSeconds(uint32_t secs)
{
    if (inhibit_second_update > 0)
    {
        MDEBUG("TimestampDecoder::updateTimestampSeconds(%d) inhibit\n", secs);
        inhibit_second_update--;
    }
    else
    {
        MDEBUG("TimestampDecoder::updateTimestampSeconds(%d) apply\n", secs);
        time_secs = secs;
    }
}

void TimestampDecoder::updateTimestampPPS(uint32_t pps)
{
    MDEBUG("TimestampDecoder::updateTimestampPPS(%f)\n", (double)pps / 16384000.0);

    if (time_pps > pps) // Second boundary crossed
    {
        MDEBUG("TimestampDecoder::updateTimestampPPS crossed second\n");

        // The second for the next eight frames will not
        // be defined by the MNSC
        inhibit_second_update = 2;
        time_secs += 1;
    }

    time_pps = pps;
}

void TimestampDecoder::updateTimestampEti(
        int framephase,
        uint16_t mnsc,
        uint32_t pps, // In units of 1/16384000 s
        int32_t fct)
{
    updateTimestampPPS(pps);
    pushMNSCData(framephase, mnsc);
    latestFCT = fct;
}

void TimestampDecoder::set_parameter(
        const std::string& parameter,
        const std::string& value)
{
    using namespace std;

    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "offset") {
        ss >> timestamp_offset;
        offset_changed = true;
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter
            << "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const std::string TimestampDecoder::get_parameter(
        const std::string& parameter) const
{
    using namespace std;

    stringstream ss;
    if (parameter == "offset") {
        ss << timestamp_offset;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

