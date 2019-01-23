# -*- coding: utf-8 -*-
#
# Automatic Gain Control
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging
import time
import numpy as np
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
from typing import Tuple

import dpd.Adapt as Adapt
import dpd.Measure as Measure

class Agc:
    """The goal of the automatic gain control is to set the
    RX gain to a value at which all received amplitudes can
    be detected. This means that the maximum possible amplitude
    should be quantized at the highest possible digital value.

    A problem we have to face, is that the estimation of the
    maximum amplitude by applying the max() function is very
    unstable. This is due to the maximum’s rareness. Therefore
    we estimate a far more robust value, such as the median,
    and then approximate the maximum amplitude from it.

    Given this, we tune the RX gain in such a way, that the
    received signal fulfills our desired property, of having
    all amplitudes properly quantized."""

    def __init__(self, measure, adapt, c):
        assert isinstance(measure, Measure.Measure)
        assert isinstance(adapt, Adapt.Adapt)
        self.measure = measure
        self.adapt = adapt
        self.min_rxgain = c.RAGC_min_rxgain
        self.max_rxgain = c.RAGC_max_rxgain
        self.rxgain = self.min_rxgain
        self.peak_to_median = 1./c.RAGC_rx_median_target

    def run(self) -> Tuple[bool, str]:
        try:
            self.adapt.set_rxgain(self.rxgain)
        except ValueError as e:
            return (False, "Setting RX gain to {} failed: {}".format(self.rxgain, e))
        time.sleep(0.5)


        # Measure
        txframe, tx_ts, rxframe, rx_ts, rx_median, tx_median = self.measure.get_samples_unaligned(short=False)

        # Estimate Maximum
        rx_peak = self.peak_to_median * rx_median
        correction_factor = 20*np.log10(1/rx_peak)
        self.rxgain = self.rxgain + correction_factor

        measurements = "RX Median {:1.4f}, estimated peak {:1.4f}, correction factor {:1.4f}, new RX gain {:1.4f}".format(
            rx_median, rx_peak, correction_factor, self.rxgain)
        logging.info(measurements)

        if self.rxgain < self.min_rxgain:
            w = "Warning: calculated RX Gain={} is lower than minimum={}. RX feedback power is too high!".format(
                self.rxgain, self.min_rxgain)
            logging.warning(w)
            return (False, "\n".join([measurements, w]))
        elif self.rxgain > self.max_rxgain:
            w = "Warning: calculated RX Gain={} is higher than maximum={}. RX feedback power should be increased.".format(
                self.rxgain, self.max_rxgain)
            logging.warning(w)
            try:
                # Reset to a low value, as we expect the user to reduce external attenuation
                self.adapt.set_rxgain(30)
            except ValueError as e:
                return (False, "\n".join([measurements, w, "Setting RX gain to {} failed: {}".format(self.rxgain, e)]))
            return (False, "\n".join([measurements, w]))
        else:
            try:
                self.adapt.set_rxgain(self.rxgain)
            except ValueError as e:
                return (False, "Setting RX gain to {} failed: {}".format(self.rxgain, e))
            time.sleep(0.5)
            return (True, measurements)

    def plot_estimates(self):
        """Plots the estimate of for Max, Median, Mean for different
        number of samples."""
        if self.c.plot_location is None:
            return

        self.adapt.set_rxgain(self.min_rxgain)
        time.sleep(1)

        dt = datetime.datetime.now().isoformat()
        fig_path = self.c.plot_location + "/" + dt + "_agc.png"
        fig, axs = plt.subplots(2, 2, figsize=(3*6,1*6))
        axs = axs.ravel()

        for j in range(5):
            txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median, tx_median = self.measure.get_samples()

            rxframe_aligned_abs = np.abs(rxframe_aligned)

            x = np.arange(100, rxframe_aligned_abs.shape[0], dtype=int)
            rx_max_until = []
            rx_median_until = []
            rx_mean_until = []
            for i in x:
                rx_max_until.append(np.max(rxframe_aligned_abs[:i]))
                rx_median_until.append(np.median(rxframe_aligned_abs[:i]))
                rx_mean_until.append(np.mean(rxframe_aligned_abs[:i]))

            axs[0].plot(x,
                    rx_max_until,
                    label="Run {}".format(j+1),
                    color=matplotlib.colors.hsv_to_rgb((1./(j+1.),0.8,0.7)),
                    linestyle="-", linewidth=0.25)
            axs[0].set_xlabel("Samples")
            axs[0].set_ylabel("Amplitude")
            axs[0].set_title("Estimation for Maximum RX Amplitude")
            axs[0].legend()

            axs[1].plot(x,
                    rx_median_until,
                    label="Run {}".format(j+1),
                    color=matplotlib.colors.hsv_to_rgb((1./(j+1.),0.9,0.7)),
                    linestyle="-", linewidth=0.25)
            axs[1].set_xlabel("Samples")
            axs[1].set_ylabel("Amplitude")
            axs[1].set_title("Estimation for Median RX Amplitude")
            axs[1].legend()
            ylim_1 = axs[1].get_ylim()

            axs[2].plot(x,
                    rx_mean_until,
                    label="Run {}".format(j+1),
                    color=matplotlib.colors.hsv_to_rgb((1./(j+1.),0.9,0.7)),
                    linestyle="-", linewidth=0.25)
            axs[2].set_xlabel("Samples")
            axs[2].set_ylabel("Amplitude")
            axs[2].set_title("Estimation for Mean RX Amplitude")
            ylim_2 = axs[2].get_ylim()
            axs[2].legend()

            axs[1].set_ylim(min(ylim_1[0], ylim_2[0]),
                            max(ylim_1[1], ylim_2[1]))

            fig.tight_layout()
            fig.savefig(fig_path)

        axs[3].hist(rxframe_aligned_abs, bins=60)
        axs[3].set_xlabel("Amplitude")
        axs[3].set_ylabel("Frequency")
        axs[3].set_title("Histogram of Amplitudes")
        axs[3].legend()

        fig.tight_layout()
        fig.savefig(fig_path)
        plt.close(fig)


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
