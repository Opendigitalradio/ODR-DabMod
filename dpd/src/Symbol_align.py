# -*- coding: utf-8 -*-
#
# DPD Computation Engine, Modulation Error Rate.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging
import numpy as np
import scipy
import matplotlib

matplotlib.use('agg')
import matplotlib.pyplot as plt


def _remove_outliers(x, stds=5):
    deviation_from_mean = np.abs(x - np.mean(x))
    inlier_idxs = deviation_from_mean < stds * np.std(x)
    x = x[inlier_idxs]
    return x


def _calc_delta_angle(fft):
    # Introduce invariance against carrier
    angles = np.angle(fft) % (np.pi / 2.)

    # Calculate Angle difference and compensate jumps
    deltas_angle = np.diff(angles)
    deltas_angle[deltas_angle > np.pi / 4.] = \
        deltas_angle[deltas_angle > np.pi / 4.] - np.pi / 2.
    deltas_angle[-deltas_angle > np.pi / 4.] = \
        deltas_angle[-deltas_angle > np.pi / 4.] + np.pi / 2.
    deltas_angle = _remove_outliers(deltas_angle)

    delta_angle = np.mean(deltas_angle)

    return delta_angle


class Symbol_align:
    """
    Find the phase offset to the start of the DAB symbols in an
    unaligned dab signal.
    """

    def __init__(self, c, plot=False):
        self.c = c
        self.plot = plot
        pass

    def _calc_offset_to_first_symbol_without_prefix(self, tx):
        tx_orig = tx[0:-self.c.T_U]
        tx_cut_prefix = tx[self.c.T_U:]

        tx_product = np.abs(tx_orig - tx_cut_prefix)
        tx_product_avg = np.correlate(
            tx_product,
            np.ones(self.c.T_C),
            mode='valid')
        tx_product_avg_min_filt = \
            scipy.ndimage.filters.minimum_filter1d(
                tx_product_avg,
                int(1.5 * self.c.T_S)
            )
        peaks = np.ravel(np.where(tx_product_avg == tx_product_avg_min_filt))

        offset = peaks[np.argmin([tx_product_avg[peak] for peak in peaks])]

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_Symbol_align.png"

            fig = plt.figure(figsize=(9, 9))

            ax = fig.add_subplot(4, 1, 1)
            ax.plot(tx_product)
            ylim = ax.get_ylim()
            for peak in peaks:
                ax.plot((peak, peak), (ylim[0], ylim[1]))
                if peak == offset:
                    ax.text(peak, ylim[0] + 0.3 * np.diff(ylim), "offset", rotation=90)
                else:
                    ax.text(peak, ylim[0] + 0.2 * np.diff(ylim), "peak", rotation=90)
            ax.set_xlabel("Sample")
            ax.set_ylabel("Conj. Product")
            ax.set_title("Difference with shifted self")

            ax = fig.add_subplot(4, 1, 2)
            ax.plot(tx_product_avg)
            ylim = ax.get_ylim()
            for peak in peaks:
                ax.plot((peak, peak), (ylim[0], ylim[1]))
                if peak == offset:
                    ax.text(peak, ylim[0] + 0.3 * np.diff(ylim), "offset", rotation=90)
                else:
                    ax.text(peak, ylim[0] + 0.2 * np.diff(ylim), "peak", rotation=90)
            ax.set_xlabel("Sample")
            ax.set_ylabel("Conj. Product")
            ax.set_title("Moving Average")

            ax = fig.add_subplot(4, 1, 3)
            ax.plot(tx_product_avg_min_filt)
            ylim = ax.get_ylim()
            for peak in peaks:
                ax.plot((peak, peak), (ylim[0], ylim[1]))
                if peak == offset:
                    ax.text(peak, ylim[0] + 0.3 * np.diff(ylim), "offset", rotation=90)
                else:
                    ax.text(peak, ylim[0] + 0.2 * np.diff(ylim), "peak", rotation=90)
            ax.set_xlabel("Sample")
            ax.set_ylabel("Conj. Product")
            ax.set_title("Min Filter")

            ax = fig.add_subplot(4, 1, 4)
            tx_product_crop = tx_product[peaks[0] - 50:peaks[0] + 50]
            x = range(tx_product.shape[0])[peaks[0] - 50:peaks[0] + 50]
            ax.plot(x, tx_product_crop)
            ylim = ax.get_ylim()
            ax.plot((peaks[0], peaks[0]), (ylim[0], ylim[1]))
            ax.set_xlabel("Sample")
            ax.set_ylabel("Conj. Product")
            ax.set_title("Difference with shifted self")

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

        # "offset" measures where the shifted signal matches the
        # original signal. Therefore we have to subtract the size
        # of the shift to find the offset of the symbol start.
        return (offset + self.c.T_C) % self.c.T_S

    def _delta_angle_to_samples(self, angle):
        return - angle / self.c.phase_offset_per_sample

    def _calc_sample_offset(self, sig):
        assert sig.shape[0] == self.c.T_U, \
            "Input length is not a Symbol without cyclic prefix"

        fft = np.fft.fftshift(np.fft.fft(sig))
        fft_crop = np.delete(fft[self.c.FFT_start:self.c.FFT_end], self.c.FFT_delete)
        delta_angle = _calc_delta_angle(fft_crop)
        delta_sample = self._delta_angle_to_samples(delta_angle)
        delta_sample_int = np.round(delta_sample).astype(int)
        error = np.abs(delta_sample_int - delta_sample)
        if error > 0.1:
            raise RuntimeError("Could not calculate " \
                               "sample offset. Error {}".format(error))
        return delta_sample_int

    def calc_offset(self, tx):
        """Calculate the offset the first symbol"""
        off_sym = self._calc_offset_to_first_symbol_without_prefix(
            tx)
        off_sam = self._calc_sample_offset(
            tx[off_sym:off_sym + self.c.T_U])
        off = (off_sym + off_sam) % self.c.T_S

        assert self._calc_sample_offset(tx[off:off + self.c.T_U]) == 0, \
            "Failed to calculate offset"
        return off

    def crop_symbol_without_cyclic_prefix(self, tx):
        off = self.calc_offset(tx)
        return tx[
               off:
               off + self.c.T_U
               ]

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
