# -*- coding: utf-8 -*-
#
# DPD Computation Engine, model implementation using polynomial
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import os
import logging
import numpy as np
import matplotlib.pyplot as plt

def assert_np_float32(array):
    assert isinstance(array, np.ndarray), type(array)
    assert array.dtype == np.float32, array.dtype
    assert array.flags.contiguous
    assert not any(np.isnan(array))


def _check_input_get_next_coefs(tx_abs, rx_abs, phase_diff):
    assert_np_float32(tx_abs)
    assert_np_float32(rx_abs)
    assert_np_float32(phase_diff)

    assert tx_abs.shape == rx_abs.shape, \
        "tx_abs.shape {}, rx_abs.shape {}".format(
            tx_abs.shape, rx_abs.shape)
    assert tx_abs.shape == phase_diff.shape, \
        "tx_abs.shape {}, phase_diff.shape {}".format(
            tx_abs.shape, phase_diff.shape)


class Poly:
    """Calculates new coefficients using the measurement and the previous
    coefficients"""

    def __init__(self, c, learning_rate_am=1.0, learning_rate_pm=1.0):
        self.c = c

        self.learning_rate_am = learning_rate_am
        self.learning_rate_pm = learning_rate_pm

        self.reset_coefs()

    def plot(self, plot_location, title):
        if self._am_plot_data is not None and self._pm_plot_data is not None:
            tx_dpd, rx_received, coefs_am, coefs_am_new = self._am_plot_data

            tx_range, rx_est = self._am_calc_line(coefs_am, 0, 0.6)
            tx_range_new, rx_est_new = self._am_calc_line(coefs_am_new, 0, 0.6)

            sub_rows = 2
            sub_cols = 1
            fig = plt.figure(figsize=(sub_cols * 6, sub_rows / 2. * 6))
            i_sub = 0

            # AM subplot
            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(tx_range, rx_est,
                    label="Estimated TX",
                    alpha=0.3,
                    color="gray")
            ax.plot(tx_range_new, rx_est_new,
                    label="New Estimated TX",
                    color="red")
            ax.scatter(tx_dpd, rx_received,
                       label="Binned Data",
                       color="blue",
                       s=1)
            ax.set_title("Model AM and PM {}".format(title))
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")
            ax.set_xlim(0, 1.0)
            ax.legend(loc=4)

            # PM sub plot
            tx_dpd, phase_diff, coefs_pm, coefs_pm_new = self._pm_plot_data

            tx_range, phase_diff_est = self._pm_calc_line(coefs_pm, 0, 0.6)
            tx_range_new, phase_diff_est_new = self._pm_calc_line(coefs_pm_new, 0, 0.6)

            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(tx_range, phase_diff_est,
                    label="Estimated Phase Diff",
                    alpha=0.3,
                    color="gray")
            ax.plot(tx_range_new, phase_diff_est_new,
                    label="New Estimated Phase Diff",
                    color="red")
            ax.scatter(tx_dpd, phase_diff,
                       label="Binned Data",
                       color="blue",
                       s=1)
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase DIff")
            ax.set_xlim(0, 1.0)
            ax.legend(loc=4)

            fig.tight_layout()
            fig.savefig(plot_location)
            plt.close(fig)

    def reset_coefs(self):
        self.coefs_am = np.zeros(5, dtype=np.float32)
        self.coefs_am[0] = 1
        self.coefs_pm = np.zeros(5, dtype=np.float32)

    def train(self, tx_abs, rx_abs, phase_diff, lr=None):
        """
        :type tx_abs: np.ndarray
        :type rx_abs: np.ndarray
        :type phase_diff: np.ndarray
        :type lr: float
        """
        _check_input_get_next_coefs(tx_abs, rx_abs, phase_diff)

        coefs_am_new = self._am_get_next_coefs(tx_abs, rx_abs, self.coefs_am)
        coefs_pm_new = self._pm_get_next_coefs(tx_abs, phase_diff, self.coefs_pm)

        self.coefs_am = self.coefs_am + (coefs_am_new - self.coefs_am) * self.learning_rate_am
        self.coefs_pm = self.coefs_pm + (coefs_pm_new - self.coefs_pm) * self.learning_rate_pm

    def get_dpd_data(self):
        return "poly", self.coefs_am, self.coefs_pm

    def set_dpd_data(self, dpddata):
        if dpddata[0] != "poly" or len(dpddata) != 3:
            raise ValueError("dpddata is not of 'poly' format")
        _, self.coefs_am, self.coefs_pm = dpddata

    def _am_calc_line(self, coefs, min_amp, max_amp):
        rx_range = np.linspace(min_amp, max_amp)
        tx_est = np.sum(self._am_poly(rx_range) * coefs, axis=1)
        return tx_est, rx_range

    def _am_poly(self, sig):
        return np.array([sig ** i for i in range(1, 6)]).T

    def _am_fit_poly(self, tx_abs, rx_abs):
        return np.linalg.lstsq(self._am_poly(rx_abs), tx_abs, rcond=None)[0]

    def _am_get_next_coefs(self, tx_dpd, rx_received, coefs_am):
        """Calculate the next AM/AM coefficients using the extracted
        statistic of TX and RX amplitude"""

        coefs_am_new = self._am_fit_poly(tx_dpd, rx_received)
        coefs_am_new = coefs_am + \
                       self.learning_rate_am * (coefs_am_new - coefs_am)

        self._am_plot_data = (tx_dpd, rx_received, coefs_am, coefs_am_new)

        return coefs_am_new

    def _pm_poly(self, sig):
        return np.array([sig ** i for i in range(0, 5)]).T

    def _pm_calc_line(self, coefs, min_amp, max_amp):
        tx_range = np.linspace(min_amp, max_amp)
        phase_diff = np.sum(self._pm_poly(tx_range) * coefs, axis=1)
        return tx_range, phase_diff

    def _discard_small_values(self, tx_dpd, phase_diff):
        """ Assumes that the phase for small tx amplitudes is zero"""
        mask = tx_dpd < self.c.MPM_tx_min
        phase_diff[mask] = 0
        return tx_dpd, phase_diff

    def _pm_fit_poly(self, tx_abs, phase_diff):
        return np.linalg.lstsq(self._pm_poly(tx_abs), phase_diff, rcond=None)[0]

    def _pm_get_next_coefs(self, tx_dpd, phase_diff, coefs_pm):
        """Calculate the next AM/PM coefficients using the extracted
        statistic of TX amplitude and phase difference"""
        tx_dpd, phase_diff = self._discard_small_values(tx_dpd, phase_diff)

        coefs_pm_new = self._pm_fit_poly(tx_dpd, phase_diff)

        coefs_pm_new = coefs_pm + self.learning_rate_pm * (coefs_pm_new - coefs_pm)
        self._pm_plot_data = (tx_dpd, phase_diff, coefs_pm, coefs_pm_new)

        return coefs_pm_new

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
# Copyright (c) 2018 Matthias P. Brandli
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
