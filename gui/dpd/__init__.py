# -*- coding: utf-8 -*-
#
# DPD Computation Engine module
#
#   Copyright (c) 2017 Andreas Steger
#   Copyright (c) 2018 Matthias P. Braendli
#
#    http://www.opendigitalradio.org
#
#   This file is part of ODR-DabMod.
#
#   ODR-DabMod is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of the
#   License, or (at your option) any later version.
#
#   ODR-DabMod is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

from . import Capture

class DPD:
    def __init__(self, samplerate=8192000):
        self.samplerate = samplerate

        oversample = int(self.samplerate / 2048000)
        self.T_F = oversample * 196608  # Transmission frame duration
        self.T_NULL = oversample * 2656  # Null symbol duration
        self.T_S = oversample * 2552  # Duration of OFDM symbols of indices l = 1, 2, 3,... L;
        self.T_U = oversample * 2048  # Inverse of carrier spacing
        self.T_C = oversample * 504  # Duration of cyclic prefix

        self.last_capture_info = {}

        port = 50055
        samples_to_capture = 81920
        self.capture = Capture.Capture(self.samplerate, port, samples_to_capture)

    def status(self):
        r = {}
        r['histogram'] = self.capture.bin_histogram()
        r['capture'] = self.last_capture_info
        return r

    def capture_samples(self):
        """Captures samples and store them in the accumulated samples,
        returns a dict with some info"""
        try:
            txframe_aligned, tx_ts, tx_median, rxframe_aligned, rx_ts, rx_median = self.capture.get_samples()
            self.last_capture_info['length'] = len(txframe_aligned)
            self.last_capture_info['tx_median'] = float(tx_median)
            self.last_capture_info['rx_median'] = float(rx_median)
            self.last_capture_info['tx_ts'] = tx_ts
            self.last_capture_info['rx_ts'] = rx_ts
            return self.last_capture_info
        except ValueError as e:
            raise ValueError("Capture failed: {}".format(e))

        # tx, rx, phase_diff, n_per_bin = extStat.extract(txframe_aligned, rxframe_aligned)
        # off = SA.calc_offset(txframe_aligned)
        # print("off {}".format(off))
        # tx_mer = MER.calc_mer(txframe_aligned[off:off + c.T_U], debug_name='TX')
        # print("tx_mer {}".format(tx_mer))
        # rx_mer = MER.calc_mer(rxframe_aligned[off:off + c.T_U], debug_name='RX')
        # print("rx_mer {}".format(rx_mer))
        # mse = np.mean(np.abs((txframe_aligned - rxframe_aligned) ** 2))
        # print("mse {}".format(mse))
