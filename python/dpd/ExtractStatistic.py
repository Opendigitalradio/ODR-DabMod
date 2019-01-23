# -*- coding: utf-8 -*-
#
# DPD Computation Engine,
# Extract statistic from received TX and RX data to use in Model
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import numpy as np
import matplotlib.pyplot as plt
import datetime
import os
import logging


def _check_input_extract(tx_dpd, rx_received):
    # Check data type
    assert tx_dpd[0].dtype == np.complex64, \
        "tx_dpd is not complex64 but {}".format(tx_dpd[0].dtype)
    assert rx_received[0].dtype == np.complex64, \
        "rx_received is not complex64 but {}".format(rx_received[0].dtype)
    # Check if signals have same normalization
    normalization_error = np.abs(np.median(np.abs(tx_dpd)) -
                                 np.median(np.abs(rx_received))) / (
                              np.median(np.abs(tx_dpd)) + np.median(np.abs(rx_received)))
    assert normalization_error < 0.01, "Non normalized signals"


def _phase_diff_value_per_bin(phase_diffs_values_lists):
    phase_list = []
    for values in phase_diffs_values_lists:
        mean = np.mean(values) if len(values) > 0 else np.nan
        phase_list.append(mean)
    return phase_list


class ExtractStatistic:
    """Calculate a low variance RX value for equally spaced tx values
    of a predefined range"""

    def __init__(self, c, peak_amplitude):
        self.c = c

        self._plot_data = None

        # Number of measurements used to extract the statistic
        self.n_meas = 0

        # Boundaries for the bins
        self.tx_boundaries = np.linspace(0.0, peak_amplitude, c.ES_n_bins + 1)
        self.n_per_bin = c.ES_n_per_bin

        # List of rx values for each bin
        self.rx_values_lists = []
        for i in range(c.ES_n_bins):
            self.rx_values_lists.append([])

        # List of tx values for each bin
        self.tx_values_lists = []
        for i in range(c.ES_n_bins):
            self.tx_values_lists.append([])

    def get_bin_info(self):
        return "Binning: {} bins used for amplitudes between {} and {}".format(
                len(self.tx_boundaries), np.min(self.tx_boundaries), np.max(self.tx_boundaries))

    def plot(self, plot_path, title):
        if self._plot_data is not None:
            tx_values, rx_values, phase_diffs_values, phase_diffs_values_lists = self._plot_data

            sub_rows = 3
            sub_cols = 1
            fig = plt.figure(figsize=(sub_cols * 6, sub_rows / 2. * 6))
            i_sub = 0

            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(tx_values, rx_values,
                    label="Averaged measurements",
                    color="red")
            for i, tx_value in enumerate(tx_values):
                rx_values_list = self.rx_values_lists[i]
                ax.scatter(np.ones(len(rx_values_list)) * tx_value,
                           np.abs(rx_values_list),
                           s=0.1,
                           color="black")
            ax.set_title("Extracted Statistic {}".format(title))
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")
            ax.set_ylim(0, np.max(self.tx_boundaries)) # we expect a rougly a 1:1 correspondence between x and y
            ax.set_xlim(0, np.max(self.tx_boundaries))
            ax.legend(loc=4)

            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(tx_values, np.rad2deg(phase_diffs_values),
                    label="Averaged measurements",
                    color="red")
            for i, tx_value in enumerate(tx_values):
                phase_diff = phase_diffs_values_lists[i]
                ax.scatter(np.ones(len(phase_diff)) * tx_value,
                           np.rad2deg(phase_diff),
                           s=0.1,
                           color="black")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase Difference")
            ax.set_ylim(-60, 60)
            ax.set_xlim(0, np.max(self.tx_boundaries))
            ax.legend(loc=4)

            num = []
            for i, tx_value in enumerate(tx_values):
                rx_values_list = self.rx_values_lists[i]
                num.append(len(rx_values_list))
            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(num)
            ax.set_xlabel("TX Amplitude bin")
            ax.set_ylabel("Number of Samples")
            ax.set_ylim(0, self.n_per_bin * 1.2)

            fig.tight_layout()
            fig.savefig(plot_path)
            plt.close(fig)

    def _rx_value_per_bin(self):
        rx_values = []
        for values in self.rx_values_lists:
            mean = np.mean(np.abs(values)) if len(values) > 0 else np.nan
            rx_values.append(mean)
        return rx_values

    def _tx_value_per_bin(self):
        tx_values = []
        for start, end in zip(self.tx_boundaries, self.tx_boundaries[1:]):
            tx_values.append(np.mean((start, end)))
        return tx_values

    def _phase_diff_list_per_bin(self):
        phase_values_lists = []
        for tx_list, rx_list in zip(self.tx_values_lists, self.rx_values_lists):
            phase_diffs = []
            for tx, rx in zip(tx_list, rx_list):
                phase_diffs.append(np.angle(rx * tx.conjugate()))
            phase_values_lists.append(phase_diffs)
        return phase_values_lists

    def extract(self, tx_dpd, rx):
        """Extract information from a new measurement and store them
        in member variables."""
        _check_input_extract(tx_dpd, rx)
        self.n_meas += 1

        tx_abs = np.abs(tx_dpd)
        for i, (tx_start, tx_end) in enumerate(zip(self.tx_boundaries, self.tx_boundaries[1:])):
            mask = (tx_abs > tx_start) & (tx_abs < tx_end)
            n_add = max(0, self.n_per_bin - len(self.rx_values_lists[i]))
            self.rx_values_lists[i] += \
                list(rx[mask][:n_add])
            self.tx_values_lists[i] += \
                list(tx_dpd[mask][:n_add])

        rx_values = self._rx_value_per_bin()
        tx_values = self._tx_value_per_bin()

        n_per_bin = np.array([len(values) for values in self.rx_values_lists])
        # Index of first not filled bin, assumes that never all bins are filled
        idx_end = np.argmin(n_per_bin == self.c.ES_n_per_bin)

        phase_diffs_values_lists = self._phase_diff_list_per_bin()
        phase_diffs_values = _phase_diff_value_per_bin(phase_diffs_values_lists)

        self._plot_data = (tx_values, rx_values, phase_diffs_values, phase_diffs_values_lists)

        tx_values_crop = np.array(tx_values, dtype=np.float32)[:idx_end]
        rx_values_crop = np.array(rx_values, dtype=np.float32)[:idx_end]
        phase_diffs_values_crop = np.array(phase_diffs_values, dtype=np.float32)[:idx_end]
        return tx_values_crop, rx_values_crop, phase_diffs_values_crop, n_per_bin

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
# Copyright (c) 2018 Matthias P. Braendli
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
