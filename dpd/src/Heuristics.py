# -*- coding: utf-8 -*-
#
# DPD Calculation Engine, heuristics we use to tune the parameters.
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
