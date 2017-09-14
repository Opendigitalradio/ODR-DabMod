# -*- coding: utf-8 -*-
#
# DPD Calculation Engine,
# Extract statistic from data to use in Model
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import numpy as np
import pickle
import matplotlib.pyplot as plt

import datetime
import os
import logging

logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)


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


class ExtractStatistic:
    """Calculate a low variance RX value for equally spaced tx values
    of a predefined range"""

    def __init__(self,
                 c,
                 plot=False):
        self.c = c

        self.tx_boundaries = np.linspace(c.ES_start, c.ES_end, c.ES_n_bins + 1)
        self.n_per_bin = c.ES_n_per_bin

        self.rx_values_lists = []
        for i in range(c.ES_n_bins):
            self.rx_values_lists.append([])

        self.tx_values_lists = []
        for i in range(c.ES_n_bins):
            self.tx_values_lists.append([])

        self.tx_values = self._tx_value_per_bin()

        self.rx_values = []
        for i in range(c.ES_n_bins):
            self.rx_values.append(None)

        self.plot = plot

    def _plot_and_log(self):
        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            phase_diffs_values_lists = self._phase_diff_list_per_bin()
            phase_diffs_values = self._phase_diff_value_per_bin(phase_diffs_values_lists)

            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_ExtractStatistic.png"
            sub_rows = 3
            sub_cols = 1
            fig = plt.figure(figsize=(sub_cols * 6, sub_rows / 2. * 6))
            i_sub = 0

            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(self.tx_values, self.rx_values,
                    label="Estimated Values",
                    color="red")
            for i, tx_value in enumerate(self.tx_values):
                rx_values = self.rx_values_lists[i]
                ax.scatter(np.ones(len(rx_values)) * tx_value,
                           np.abs(rx_values),
                           s=0.1,
                           color="black")
            ax.set_title("Extracted Statistic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")
            ax.set_ylim(0, 0.8)
            ax.set_xlim(0, 1.1)
            ax.legend(loc=4)

            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(self.tx_values, np.rad2deg(phase_diffs_values),
                    label="Estimated Values",
                    color="red")
            for i, tx_value in enumerate(self.tx_values):
                phase_diff = phase_diffs_values_lists[i]
                ax.scatter(np.ones(len(phase_diff)) * tx_value,
                           np.rad2deg(phase_diff),
                           s=0.1,
                           color="black")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase Difference")
            ax.set_ylim(-60,60)
            ax.set_xlim(0, 1.1)
            ax.legend(loc=4)

            num = []
            for i, tx_value in enumerate(self.tx_values):
                rx_values = self.rx_values_lists[i]
                num.append(len(rx_values))
            i_sub += 1
            ax = plt.subplot(sub_rows, sub_cols, i_sub)
            ax.plot(num)
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Number of Samples")
            ax.set_ylim(0, self.n_per_bin * 1.2)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

            pickle.dump(self.rx_values_lists, open("/tmp/rx_values", "wb"))
            pickle.dump(self.tx_values, open("/tmp/tx_values", "wb"))

    def _rx_value_per_bin(self):
        rx_values = []
        for values in self.rx_values_lists:
            rx_values.append(np.mean(np.abs(values)))
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

    def _phase_diff_value_per_bin(self, phase_diffs_values_lists):
        phase_list = []
        for values in phase_diffs_values_lists:
            phase_list.append(np.mean(values))
        return phase_list

    def extract(self, tx_dpd, rx):
        _check_input_extract(tx_dpd, rx)

        tx_abs = np.abs(tx_dpd)
        for i, (tx_start, tx_end) in enumerate(zip(self.tx_boundaries, self.tx_boundaries[1:])):
            mask = (tx_abs > tx_start) & (tx_abs < tx_end)
            n_add = max(0, self.n_per_bin - len(self.rx_values_lists[i]))
            self.rx_values_lists[i] += \
                list(rx[mask][:n_add])
            self.tx_values_lists[i] += \
                list(tx_dpd[mask][:n_add])

        self.rx_values = self._rx_value_per_bin()
        self.tx_values = self._tx_value_per_bin()

        self._plot_and_log()

        n_per_bin = [len(values) for values in self.rx_values_lists]

        # TODO cleanup
        phase_diffs_values_lists = self._phase_diff_list_per_bin()
        phase_diffs_values = self._phase_diff_value_per_bin(phase_diffs_values_lists)

        return np.array(self.tx_values,  dtype=np.float32), \
               np.array(self.rx_values, dtype=np.float32), \
               np.array(phase_diffs_values, dtype=np.float32), \
               n_per_bin

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
