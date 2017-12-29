# -*- coding: utf-8 -*-
#
# DPD Computation Engine, model implementation using polynomial
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import os
import logging
import numpy as np

import src.Model_AM as Model_AM
import src.Model_PM as Model_PM


def assert_np_float32(x):
    assert isinstance(x, np.ndarray)
    assert x.dtype == np.float32
    assert x.flags.contiguous


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

    def __init__(self,
                 c,
                 learning_rate_am=1.0,
                 learning_rate_pm=1.0):
        self.c = c
        self.plot = c.MDL_plot

        self.learning_rate_am = learning_rate_am
        self.learning_rate_pm = learning_rate_pm

        self.reset_coefs()

        self.model_am = Model_AM.Model_AM(c, plot=self.plot)
        self.model_pm = Model_PM.Model_PM(c, plot=self.plot)

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

        if not lr is None:
            self.model_am.learning_rate_am = lr
            self.model_pm.learning_rate_pm = lr

        coefs_am_new = self.model_am.get_next_coefs(tx_abs, rx_abs, self.coefs_am)
        coefs_pm_new = self.model_pm.get_next_coefs(tx_abs, phase_diff, self.coefs_pm)

        self.coefs_am = self.coefs_am + (coefs_am_new - self.coefs_am) * self.learning_rate_am
        self.coefs_pm = self.coefs_pm + (coefs_pm_new - self.coefs_pm) * self.learning_rate_pm

    def get_dpd_data(self):
        return "poly", self.coefs_am, self.coefs_pm

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
