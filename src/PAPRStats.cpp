/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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

#include "PAPRStats.h"
#include <numeric>
#include <cmath>
#include <stdexcept>
#if defined(TEST)
/* compile with g++ -std=c++11 -Wall -DTEST PAPRStats.cpp -o paprtest */
#  include <iostream>
#endif


PAPRStats::PAPRStats(size_t num_blocks_to_accumulate) :
    m_num_blocks_to_accumulate(num_blocks_to_accumulate)
{
}

void PAPRStats::process_block(const complexf* data, size_t data_len)
{
    double norm_peak = 0;
    double rms2 = 0;

    for (size_t i = 0; i < data_len; i++) {
        const double x_norm = std::norm(data[i]);

        if (x_norm > norm_peak) {
            norm_peak = x_norm;
        }

        rms2 += x_norm;
    }

    rms2 /= data_len;

#if defined(TEST)
    std::cerr << "Accumulating peak " << norm_peak <<
        " rms2 " << rms2 << std::endl;
#endif

    m_squared_peaks.push_back(norm_peak);
    m_squared_mean.push_back(rms2);

    if (m_squared_mean.size() > m_num_blocks_to_accumulate) {
        m_squared_mean.pop_front();
        m_squared_peaks.pop_front();
    }
}

double PAPRStats::calculate_papr() const
{
    if (m_squared_mean.size() < m_num_blocks_to_accumulate) {
        return 0;
    }

    if (m_squared_mean.size() != m_squared_peaks.size()) {
        throw std::logic_error("Invalid PAPR measurement sizes");
    }

    double peak = 0;
    double rms2 = 0;
    for (size_t i = 0; i < m_squared_peaks.size(); i++) {
        if (m_squared_peaks[i] > peak) {
            peak = m_squared_peaks[i];
        }

        rms2 += m_squared_mean[i];
    }

    // This assumes all blocks given to process have the same length
    rms2 /= m_squared_peaks.size();

#if defined(TEST)
    std::cerr << "Calculate peak " << peak <<
        " rms2 " << rms2 << std::endl;
#endif

    return 10.0 * std::log10(peak / rms2);
}

void PAPRStats::clear()
{
    m_squared_peaks.clear();
    m_squared_mean.clear();
}

#if defined(TEST)
/* Test python code:
import numpy as np
vec = 0.5 * np.exp(np.complex(0, 0.3) * np.arange(40))
vec[26] = 10.0 * vec[26]
rms = np.mean(vec * np.conj(vec)).real
peak = np.amax(vec * np.conj(vec)).real
print("rms {}".format(rms))
print("peak {}".format(peak))
print(10. * np.log10(peak / rms))
*/
int main(int argc, char **argv)
{
    using namespace std;
    vector<complexf> vec(40);

    for (size_t i = 0; i < vec.size(); i++) {
        vec[i] = polar(0.5, 0.3 * i);
        if (i == 26) {
            vec[i] *= 10;
        }
        cout << " " << vec[i];
    }
    cout << endl;

    PAPRStats stats(4);

    for (size_t i = 0; i < 3; i++) {
        stats.process_block(vec.data(), vec.size());
    }

    const auto papr0 = stats.calculate_papr();
    if (papr0 != 0) {
        cerr << "Expected 0, got " << papr0 << endl;
    }

    stats.process_block(vec.data(), vec.size());

    const auto papr1 = stats.calculate_papr();
    cout << "PAPR = " << papr1 << " dB" << endl;

}

#endif
