/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

    if (m_conf.pps_src == "gpsdo") {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto expiry = now + seconds(m_conf.maxGPSHoldoverTime);
        auto checkfunc = gpsdo_is_ettus() ? check_gps_locked : check_gps_timelock;
        while (now < expiry) {
            if (checkfunc(m_usrp)) {
                break;
            }

            now = system_clock::now();
            this_thread::sleep_for(seconds(1));
        }

        if (not checkfunc(m_usrp)) {
            throw runtime_error("GPSDO did not lock during startup");
        }
    }

    if (m_conf.pps_src == "none") {
        if (m_conf.enableSync) {
            etiLog.level(warn) <<
                "OutputUHD: WARNING:"
                " you are using synchronous transmission without PPS input!";
        }

        set_usrp_time_from_localtime();
    }
    else if (m_conf.pps_src == "pps" or m_conf.pps_src == "gpsdo") {
        set_usrp_time_from_pps();
    }
    else {
        throw std::runtime_error("USRPTime not implemented yet: " +
                m_conf.pps_src);
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

/* Return a uhd:time_spec representing current system time
 * with 1ms granularity.  */
static uhd::time_spec_t uhd_timespec_now(void)
{
    using namespace std::chrono;
    auto n = system_clock::now();
    const long long ticks = duration_cast<milliseconds>(n.time_since_epoch()).count();
    return uhd::time_spec_t::from_ticks(ticks, 1000);
}

void USRPTime::set_usrp_time_from_localtime()
{
    const auto t = uhd_timespec_now();
    m_usrp->set_time_now(t);

    etiLog.level(info) << "OutputUHD: Setting USRP time to " <<
        std::fixed << t.get_real_secs();
}

void USRPTime::set_usrp_time_from_pps()
{
    using namespace std::chrono;

    /* handling time for synchronisation: wait until the next full
     * second, and set the USRP time at next PPS */
    auto now = uhd_timespec_now();
    const time_t secs_since_epoch = now.get_full_secs();

    while (secs_since_epoch + 1 > now.get_full_secs()) {
        this_thread::sleep_for(milliseconds(1));
        now = uhd_timespec_now();
    }
    /* We are now shortly after the second change.
     * Wait 200ms to ensure the PPS comes later. */
    this_thread::sleep_for(milliseconds(200));

    const auto time_set = uhd::time_spec_t(secs_since_epoch + 3, 0.0);
    etiLog.level(info) << "OutputUHD: Setting USRP time next pps to " <<
        std::fixed << time_set.get_real_secs();
    m_usrp->set_time_unknown_pps(time_set);

    // The UHD doc says we need to give the USRP one second to update
    // all the internal registers.
    this_thread::sleep_for(seconds(1));
    const auto time_usrp = m_usrp->get_time_now();
    etiLog.level(info) << "OutputUHD: USRP time " <<
        std::fixed << time_usrp.get_real_secs();

    if (std::abs(time_usrp.get_real_secs() - time_set.get_real_secs()) > 10.0) {
        throw runtime_error("OutputUHD: Unable to set USRP time!");
    }
}

} // namespace Output

#endif // HAVE_OUTPUT_UHD
