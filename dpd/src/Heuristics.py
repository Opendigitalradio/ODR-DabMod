# -*- coding: utf-8 -*-
#
# DPD Calculation Engine, heuristics we use to tune the parameters
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import numpy as np

def get_learning_rate(idx_run):
    idx_max = 10.0
    lr_min = 0.05
    lr_max = 1
    lr_delta = lr_max - lr_min
    idx_run = min(idx_run, idx_max)
    learning_rate = lr_max - lr_delta * idx_run/idx_max
    return learning_rate

def get_n_meas(idx_run):
    idx_max = 10.0
    n_meas_min = 5
    n_meas_max = 50
    n_meas_delta = n_meas_max - n_meas_min
    idx_run = min(idx_run, idx_max)
    learning_rate = n_meas_delta * idx_run/idx_max + n_meas_min
    return int(np.round(learning_rate))


