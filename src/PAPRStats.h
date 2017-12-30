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

#pragma once

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <cstddef>
#include <vector>
#include <deque>
#include <complex>

typedef std::complex<float> complexf;

/* Helper class to calculate Peak-to-average-power ratio.
 * Definition of PAPR:
 *
 * PAPR_dB = 10 * log_10 ( abs(x_peak)^2 / x_rms^2 )
 *
 * with abs(x_peak) the peak amplitude of the signal, and
 * x_rms the Root Mean Squared.
 *
 * x_rms^2 = 1/n * Sum abs(x_n)^2
 *         = 1/n * Sum norm(x_n)
 *
 * Given that peaks are rare in a DAB signal, we want to accumulate
 * several seconds worth of samples to do our calculation.
 */
class PAPRStats
{
    public:
        PAPRStats(size_t num_blocks_to_accumulate);

        /* Push in a new block of samples to measure. calculate_papr()
         * assumes all blocks have the same size.
         */
        void process_block(const complexf* data, size_t data_len);

        /* Returns PAPR in dB if enough blocks were processed, or
         * 0 otherwise.
         */
        double calculate_papr(void) const;

        void clear(void);

    private:
        size_t m_num_blocks_to_accumulate;
        std::deque<double> m_squared_peaks;
        std::deque<double> m_squared_mean;
};



