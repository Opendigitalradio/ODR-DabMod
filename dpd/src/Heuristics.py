# -*- coding: utf-8 -*-
#
# DPD Computation Engine, heuristics we use to tune the parameters.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import numpy as np


def get_learning_rate(idx_run):
    """Gradually reduce learning rate from lr_max to lr_min within
     idx_max steps, then keep the learning rate at lr_min"""
    idx_max = 10.0
    lr_min = 0.05
    lr_max = 0.4
    lr_delta = lr_max - lr_min
    idx_run = min(idx_run, idx_max)
    learning_rate = lr_max - lr_delta * idx_run / idx_max
    return learning_rate


def get_n_meas(idx_run):
    """Gradually increase number of measurements used to extract
    a statistic from n_meas_min to n_meas_max within idx_max steps,
    then keep number of measurements at n_meas_max"""
    idx_max = 10.0
    n_meas_min = 10
    n_meas_max = 20
    n_meas_delta = n_meas_max - n_meas_min
    idx_run = min(idx_run, idx_max)
    learning_rate = n_meas_delta * idx_run / idx_max + n_meas_min
    return int(np.round(learning_rate))

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
# Copyright (c) 2017 Matthias P. Braendli
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
