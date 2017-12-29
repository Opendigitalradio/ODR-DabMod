# -*- coding: utf-8 -*-
#
# DPD Computation Engine, calculate peak to shoulder difference.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging
import multiprocessing
import numpy as np
import matplotlib.pyplot as plt


def plt_next_axis(sub_rows, sub_cols, i_sub):
    i_sub += 1
    ax = plt.subplot(sub_rows, sub_cols, i_sub)
    return i_sub, ax


def plt_annotate(ax, x, y, title=None, legend_loc=None):
    ax.set_xlabel(x)
    ax.set_ylabel(y)
    if title is not None:
        ax.set_title(title)
    if legend_loc is not None:
        ax.legend(loc=legend_loc)


def calc_fft_db(signal, offset, c):
    fft = np.fft.fftshift(np.fft.fft(signal[offset:offset + c.MS_FFT_size]))
    fft_db = 20 * np.log10(np.abs(fft))
    return fft_db


def _calc_peak(fft, c):
    assert fft.shape == (c.MS_FFT_size,), fft.shape
    idxs = (c.MS_peak_start, c.MS_peak_end)
    peak = np.mean(fft[idxs[0]:idxs[1]])
    return peak, idxs


def _calc_shoulder_hight(fft_db, c):
    assert fft_db.shape == (c.MS_FFT_size,), fft_db.shape
    idxs_left = (c.MS_shoulder_left_start, c.MS_shoulder_left_end)
    idxs_right = (c.MS_shoulder_right_start, c.MS_shoulder_right_end)

    shoulder_left = np.mean(fft_db[idxs_left[0]:idxs_left[1]])
    shoulder_right = np.mean(fft_db[idxs_right[0]:idxs_right[1]])

    shoulder = np.mean((shoulder_left, shoulder_right))
    return shoulder, (idxs_left, idxs_right)


def calc_shoulder(fft, c):
    peak = _calc_peak(fft, c)[0]
    shoulder = _calc_shoulder_hight(fft, c)[0]
    assert (peak >= shoulder), (peak, shoulder)
    return peak, shoulder


def shoulder_from_sig_offset(arg):
    signal, offset, c = arg
    fft_db = calc_fft_db(signal, offset, c)
    peak, shoulder = calc_shoulder(fft_db, c)
    return peak - shoulder, peak, shoulder


class Measure_Shoulders:
    """Calculate difference between the DAB signal and the shoulder hight in the
    power spectrum"""

    def __init__(self, c):
        self.c = c
        self.plot = c.MS_plot

    def _plot(self, signal):
        if self.c.plot_location is None:
            return

        dt = datetime.datetime.now().isoformat()
        fig_path = self.c.plot_location + "/" + dt + "_sync_subsample_aligned.png"

        fft = calc_fft_db(signal, 100, self.c)
        peak, idxs_peak = _calc_peak(fft, self.c)
        shoulder, idxs_sh = _calc_shoulder_hight(fft, self.c)

        sub_rows = 1
        sub_cols = 1
        fig = plt.figure(figsize=(sub_cols * 6, sub_rows / 2. * 6))
        i_sub = 0

        i_sub, ax = plt_next_axis(sub_rows, sub_cols, i_sub)
        ax.scatter(np.arange(fft.shape[0]), fft, s=0.1,
                   label="FFT",
                   color="red")
        ax.plot(idxs_peak, (peak, peak))
        ax.plot(idxs_sh[0], (shoulder, shoulder), color='blue')
        ax.plot(idxs_sh[1], (shoulder, shoulder), color='blue')
        plt_annotate(ax, "Frequency", "Magnitude [dB]", None, 4)

        ax.text(100, -17, str(calc_shoulder(fft, self.c)))

        ax.set_ylim(-20, 30)
        fig.tight_layout()
        fig.savefig(fig_path)
        plt.close(fig)

    def average_shoulders(self, signal, n_avg=None):
        if not self.c.MS_enable:
            logging.info("Shoulder Measurement disabled via Const.py")
            return None

        assert signal.shape[0] > 4 * self.c.MS_FFT_size
        if n_avg is None:
            n_avg = self.c.MS_averaging_size

        off_min = 0
        off_max = signal.shape[0] - self.c.MS_FFT_size
        offsets = np.linspace(off_min, off_max, num=n_avg, dtype=int)

        args = zip(
            [signal, ] * offsets.shape[0],
            offsets,
            [self.c, ] * offsets.shape[0]
        )

        pool = multiprocessing.Pool(self.c.MS_n_proc)
        res = pool.map(shoulder_from_sig_offset, args)
        shoulders_diff, shoulders, peaks = zip(*res)

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            self._plot(signal)

        return np.mean(shoulders_diff), np.mean(shoulders), np.mean(peaks)

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
