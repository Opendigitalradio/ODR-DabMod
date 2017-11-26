/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

DESCRIPTION:
   The part of the UHD output that takes care of the GPSDO.
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

#include "output/USRPTime.h"

#ifdef HAVE_OUTPUT_UHD

//#define MDEBUG(fmt, args...) fprintf(LOG, fmt , ## args)
#define MDEBUG(fmt, args...)

namespace Output {

using namespace std;


// Check function for GPS TIMELOCK sensor from the ODR LEA-M8F board GPSDO
static bool check_gps_timelock(uhd::usrp::multi_usrp::sptr& usrp)
{
    try {
        const string sensor_value =
            usrp->get_mboard_sensor("gps_timelock", 0).to_pp_string();

        if (sensor_value.find("TIME LOCKED") == string::npos) {
            etiLog.level(warn) << "OutputUHD: gps_timelock " << sensor_value;
            return false;
        }

        return true;
    }
    catch (const uhd::lookup_error &e) {
        etiLog.level(warn) << "OutputUHD: no gps_timelock sensor";
        return false;
    }
}

// Check function for GPS LOCKED sensor from the Ettus GPSDO
static bool check_gps_locked(uhd::usrp::multi_usrp::sptr& usrp)
{
    try {
        const uhd::sensor_value_t sensor_value(
                usrp->get_mboard_sensor("gps_locked", 0));
        if (not sensor_value.to_bool()) {
            etiLog.level(warn) << "OutputUHD: gps_locked " <<
                sensor_value.to_pp_string();
            return false;
        }

        return true;
    }
    catch (const uhd::lookup_error &e) {
        etiLog.level(warn) << "OutputUHD: no gps_locked sensor";
        return false;
    }
}


USRPTime::USRPTime(
        uhd::usrp::multi_usrp::sptr usrp,
        SDRDeviceConfig& conf) :
    m_usrp(usrp),
    m_conf(conf),
    time_last_check(timepoint_t::clock::now())
{
    if (m_conf.enableSync and (m_conf.pps_src == "none")) {
        set_usrp_time_from_localtime();
    }
}

bool USRPTime::verify_time()
{
    if (not gpsfix_needs_check()) {
        return true;
    }

    /* During bootup, we say the gpsdo is not ok, and we poll the GPSDO until
     * we reach lock. Then we sync time. If we do not reach lock in time, we
     * crash.
     *
     * Once we are synced and we have lock, everything ok. If we lose lock for
     * a number of seconds, we switch to the lost_fix state.
     *
     * In the lost fix state, we return false to get the TX muted, and we monitor.
     * If the fix comes back, we unmute. If we reach the timeout, we crash.
     */

    check_gps();

    const auto duration_without_fix =
        gps_fix_check_interval * num_checks_without_gps_fix;

    switch (gps_state) {
        case gps_state_e::bootup:
            if (duration_without_fix > initial_gps_fix_wait) {
                throw runtime_error("GPS did not fix in " +
                        to_string(initial_gps_fix_wait) + " seconds");
            }

            if (num_checks_without_gps_fix == 0) {
                if (m_conf.pps_src != "none") {
                    set_usrp_time_from_pps();
                }
                gps_state = gps_state_e::monitor_fix;
                return true;
            }

            return false;

        case gps_state_e::monitor_fix:
            if (duration_without_fix > m_conf.maxGPSHoldoverTime) {
                throw runtime_error("Lost GPS Fix for " +
                        to_string(duration_without_fix) + " seconds");
            }

            return true;
    }

    throw logic_error("End of USRPTime::verify_time() reached");
}

void USRPTime::check_gps()
{
    timepoint_t time_now = timepoint_t::clock::now();

    // Divide interval by two because we alternate between
    // launch and check
    const auto checkinterval = chrono::seconds(lrint(gps_fix_check_interval/2.0));

    if (gpsfix_needs_check() and time_last_check + checkinterval < time_now) {
        time_last_check = time_now;

        // Alternate between launching thread and checking the
        // result.
        if (gps_fix_task.joinable()) {
            if (gps_fix_future.has_value()) {

                gps_fix_future.wait();

                gps_fix_task.join();

                if (not gps_fix_future.get()) {
                    if (num_checks_without_gps_fix == 0) {
                        etiLog.level(alert) << "OutputUHD: GPS Time Lock lost";
                    }
                    num_checks_without_gps_fix++;
                }
                else {
                    if (num_checks_without_gps_fix) {
                        etiLog.level(info) << "OutputUHD: GPS Time Lock recovered";
                    }
                    num_checks_without_gps_fix = 0;
                }
            }
        }
        else {
            // Checking the sensor here takes too much
            // time, it has to be done in a separate thread.
            if (gpsdo_is_ettus()) {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_locked, m_usrp) );
            }
            else {
                gps_fix_pt = boost::packaged_task<bool>(
                        boost::bind(check_gps_timelock, m_usrp) );
            }
            gps_fix_future = gps_fix_pt.get_future();

            gps_fix_task = boost::thread(boost::move(gps_fix_pt));
        }
    }
}

bool USRPTime::gpsfix_needs_check() const
{
    if (m_conf.refclk_src == "internal") {
        return false;
    }
    else if (m_conf.refclk_src == "gpsdo") {
        return (m_conf.maxGPSHoldoverTime != 0);
    }
    else if (m_conf.refclk_src == "gpsdo-ettus") {
        return (m_conf.maxGPSHoldoverTime != 0);
    }
    else {
        return false;
    }
}

bool USRPTime::gpsdo_is_ettus() const
{
    return (m_conf.refclk_src == "gpsdo-ettus");
}

void USRPTime::set_usrp_time_from_localtime()
{
    etiLog.level(warn) <<
        "OutputUHD: WARNING:"
        " you are using synchronous transmission without PPS input!";

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now)) {
        etiLog.level(error) << "OutputUHD: could not get time :" <<
            strerror(errno);
    }
    else {
        const uhd::time_spec_t t(now.tv_sec, (double)now.tv_nsec / 1e9);
        m_usrp->set_time_now(t);
        etiLog.level(info) << "OutputUHD: Setting USRP time to " <<
            std::fixed << t.get_real_secs();
    }
}

void USRPTime::set_usrp_time_from_pps()
{
    /* handling time for synchronisation: wait until the next full
     * second, and set the USRP time at next PPS */
    struct timespec now;
    time_t seconds;
    if (clock_gettime(CLOCK_REALTIME, &now)) {
        etiLog.level(error) << "OutputUHD: could not get time :" <<
            strerror(errno);
        throw std::runtime_error("OutputUHD: could not get time.");
    }
    else {
        seconds = now.tv_sec;

        MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
        while (seconds + 1 > now.tv_sec) {
            usleep(1);
            if (clock_gettime(CLOCK_REALTIME, &now)) {
                etiLog.level(error) << "OutputUHD: could not get time :" <<
                    strerror(errno);
                throw std::runtime_error("OutputUHD: could not get time.");
            }
        }
        MDEBUG("OutputUHD:sec+1: %ld ; now: %ld ...\n", seconds+1, now.tv_sec);
        /* We are now shortly after the second change. */

        usleep(200000); // 200ms, we want the PPS to be later
        m_usrp->set_time_unknown_pps(uhd::time_spec_t(seconds + 2));
        etiLog.level(info) << "OutputUHD: Setting USRP time next pps to " <<
            std::fixed <<
            uhd::time_spec_t(seconds + 2).get_real_secs();
    }

    usleep(1e6);
    etiLog.log(info,  "OutputUHD: USRP time %f\n",
            m_usrp->get_time_now().get_real_secs());
}


} // namespace Output

#endif // HAVE_OUTPUT_UHD
