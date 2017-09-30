# -*- coding: utf-8 -*-
#
# DPD Computation Engine, model implementation for Amplitude and not Phase
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging

logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)

import numpy as np
import matplotlib.pyplot as plt


def is_npfloat32(array):
    assert isinstance(array, np.ndarray), type(array)
    assert array.dtype == np.float32, array.dtype
    assert array.flags.contiguous
    assert not any(np.isnan(array))


def check_input_get_next_coefs(tx_dpd, phase_diff):
    is_npfloat32(tx_dpd)
    is_npfloat32(phase_diff)


class Model_PM:
    """Calculates new coefficients using the measurement and the previous
    coefficients"""

    def __init__(self,
                 c,
                 learning_rate_pm=1,
                 plot=False):
        self.c = c

        self.learning_rate_pm = learning_rate_pm
        self.plot = plot

    def _plot(self, tx_dpd, phase_diff, coefs_pm, coefs_pm_new):
        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            tx_range, phase_diff_est = self.calc_line(coefs_pm, 0, 0.6)
            tx_range_new, phase_diff_est_new = self.calc_line(coefs_pm_new, 0, 0.6)

            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_Model_PM.svg"
            sub_rows = 1
            sub_cols = 1
            fig = plt.figure(figsize=(sub_cols * 6, sub_rows / 2. * 6))
            i_sub = 0

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
            ax.set_title("Model_PM")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase DIff")
            ax.legend(loc=4)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

    def _discard_small_values(self, tx_dpd, phase_diff):
        """ Assumes that the phase for small tx amplitudes is zero"""
        mask = tx_dpd < self.c.MPM_tx_min
        phase_diff[mask] = 0
        return tx_dpd, phase_diff

    def poly(self, sig):
        return np.array([sig ** i for i in range(0, 5)]).T

    def fit_poly(self, tx_abs, phase_diff):
        return np.linalg.lstsq(self.poly(tx_abs), phase_diff)[0]

    def calc_line(self, coefs, min_amp, max_amp):
        tx_range = np.linspace(min_amp, max_amp)
        phase_diff = np.sum(self.poly(tx_range) * coefs, axis=1)
        return tx_range, phase_diff

    def get_next_coefs(self, tx_dpd, phase_diff, coefs_pm):
        """Calculate the next AM/PM coefficients using the extracted
        statistic of TX amplitude and phase difference"""
        tx_dpd, phase_diff = self._discard_small_values(tx_dpd, phase_diff)
        check_input_get_next_coefs(tx_dpd, phase_diff)

        coefs_pm_new = self.fit_poly(tx_dpd, phase_diff)

        coefs_pm_new = coefs_pm + self.learning_rate_pm * (coefs_pm_new - coefs_pm)
        self._plot(tx_dpd, phase_diff, coefs_pm, coefs_pm_new)

        return coefs_pm_new

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
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
