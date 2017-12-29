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
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt

class MER:
    def __init__(self, c):
        self.c = c

        self.plot = c.MER_plot

    def _calc_spectrum(self, tx):
        fft = np.fft.fftshift(np.fft.fft(tx))
        return np.delete(fft[self.c.FFT_start:self.c.FFT_end],
                         self.c.FFT_delete)

    def _split_in_carrier(self, x, y):
        if 0.5 < np.mean((np.abs(np.abs(x) - np.abs(y)) /
                              np.abs(np.abs(x) + np.abs(y)))):
            # Constellation points are axis aligned on the Im/Re plane
            x1 = x[(y < x) & (y > -x)]
            y1 = y[(y < x) & (y > -x)]

            x2 = x[(y > x) & (y > -x)]
            y2 = y[(y > x) & (y > -x)]

            x3 = x[(y > x) & (y < -x)]
            y3 = y[(y > x) & (y < -x)]

            x4 = x[(y < x) & (y < -x)]
            y4 = y[(y < x) & (y < -x)]
        else:
            # Constellation points are on the diagonal or Im/Re plane
            x1 = x[(+x > 0) & (+y > 0)]
            y1 = y[(+x > 0) & (+y > 0)]

            x2 = x[(-x > 0) & (+y > 0)]
            y2 = y[(-x > 0) & (+y > 0)]

            x3 = x[(-x > 0) & (-y > 0)]
            y3 = y[(-x > 0) & (-y > 0)]

            x4 = x[(+x > 0) & (-y > 0)]
            y4 = y[(+x > 0) & (-y > 0)]
        return (x1, y1), (x2, y2), (x3, y3), (x4, y4)

    def _calc_mer_for_isolated_constellation_point(self, x, y):
        """Calculate MER for one constellation point"""
        x_mean = np.mean(x)
        y_mean = np.mean(y)

        U_RMS = np.sqrt(x_mean ** 2 + y_mean ** 2)
        U_ERR = np.mean(np.sqrt((x - x_mean) ** 2 + (y - y_mean) ** 2))
        MER = 20 * np.log10(U_ERR / U_RMS)

        return x_mean, y_mean, U_RMS, U_ERR, MER

    def calc_mer(self, tx, debug_name=""):
        """Calculate MER for input signal from a symbol aligned signal."""
        assert tx.shape[0] == self.c.T_U, "Wrong input length"

        spectrum = self._calc_spectrum(tx)

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_MER" + debug_name + ".png"
        else:
            fig_path = None

        MERs = []
        for i, (x, y) in enumerate(self._split_in_carrier(
                np.real(spectrum),
                np.imag(spectrum))):
            x_mean, y_mean, U_RMS, U_ERR, MER =\
                self._calc_mer_for_isolated_constellation_point(x, y)
            MERs.append(MER)

            tau = np.linspace(0, 2 * np.pi, num=100)
            x_err = U_ERR * np.sin(tau) + x_mean
            y_err = U_ERR * np.cos(tau) + y_mean

            if self.plot:
                ax = plt.subplot(221 + i)
                ax.scatter(x, y, s=0.2, color='black')
                ax.plot(x_mean, y_mean, 'p', color='red')
                ax.plot(x_err, y_err, linewidth=2, color='blue')
                ax.text(0.1, 0.1, "MER {:.1f}dB".format(MER), transform=ax.transAxes)
                ax.set_xlabel("Real")
                ax.set_ylabel("Imag")
                ylim = ax.get_ylim()
                ax.set_ylim(ylim[0] - (ylim[1] - ylim[0]) * 0.1, ylim[1])

        if fig_path is not None:
            plt.tight_layout()
            plt.savefig(fig_path)
            plt.show()
            plt.close()

        MER_res = 20 * np.log10(np.mean([10 ** (MER / 20) for MER in MERs]))
        return MER_res

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
