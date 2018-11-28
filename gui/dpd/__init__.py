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
import numpy as np

class DPD:
    def __init__(self, plot_dir, samplerate=8192000):
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
        self.capture = Capture.Capture(self.samplerate, port, samples_to_capture, plot_dir)

    def status(self):
        r = {}
        r['histogram'] = self.capture.bin_histogram()
        r['capture'] = self.last_capture_info
        return r

    def pointcloud_png(self):
        return self.capture.pointcloud_png()

    def clear_accumulated(self):
        return self.capture.clear_accumulated()

    def capture_calibration(self):
        tx_ts, tx_median, rx_ts, rx_median, coarse_offset, correlation_coefficient = self.capture.calibrate()
        result = {'status': "ok"}
        result['tx_median'] = "{:.2f}dB".format(20*np.log10(tx_median))
        result['rx_median'] = "{:.2f}dB".format(20*np.log10(rx_median))
        result['tx_ts'] = tx_ts
        result['rx_ts'] = rx_ts
        result['coarse_offset'] = int(coarse_offset)
        result['correlation'] = float(correlation_coefficient)
        return result

    def capture_samples(self):
        """Captures samples and store them in the accumulated samples,
        returns a dict with some info"""
        result = {}
        try:
            txframe_aligned, tx_ts, tx_median, rxframe_aligned, rx_ts, rx_median = self.capture.get_samples()
            result['status'] = "ok"
            result['length'] = len(txframe_aligned)
            result['tx_median'] = float(tx_median)
            result['rx_median'] = float(rx_median)
            result['tx_ts'] = tx_ts
            result['rx_ts'] = rx_ts
        except ValueError as e:
            result['status'] = "Capture failed: {}".format(e)

        self.last_capture_info = result

        # tx, rx, phase_diff, n_per_bin = extStat.extract(txframe_aligned, rxframe_aligned)
        # off = SA.calc_offset(txframe_aligned)
        # print("off {}".format(off))
        # tx_mer = MER.calc_mer(txframe_aligned[off:off + c.T_U], debug_name='TX')
        # print("tx_mer {}".format(tx_mer))
        # rx_mer = MER.calc_mer(rxframe_aligned[off:off + c.T_U], debug_name='RX')
        # print("rx_mer {}".format(rx_mer))
        # mse = np.mean(np.abs((txframe_aligned - rxframe_aligned) ** 2))
        # print("mse {}".format(mse))
