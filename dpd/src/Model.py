# -*- coding: utf-8 -*-
#
# DPD Calculation Engine, model implementation.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging

logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)

import numpy as np
import matplotlib.pyplot as plt
from sklearn import linear_model

class PolyModel:
    """Calculates new coefficients using the measurement and the old
    coefficients"""

    def __init__(self,
                 c,
                 SA,
                 MER,
                 coefs_am,
                 coefs_pm,
                 learning_rate_am=1.,
                 learning_rate_pm=1.,
                 plot=False):
        logging.debug("Initialising Poly Model")
        self.c = c
        self.SA = SA
        self.MER = MER

        self.learning_rate_am = learning_rate_am
        self.learning_rate_pm = learning_rate_pm

        if coefs_am is None:
            self.coefs_am = [1.0, 0, 0, 0, 0]
        else:
            self.coefs_am = coefs_am
        self.coefs_am_history = [coefs_am, ]
        self.mses_am = []
        self.errs_am = []

        self.tx_mers = []
        self.rx_mers = []

        if coefs_pm is None:
            self.coefs_pm = [0, 0, 0, 0, 0]
        else:
            self.coefs_pm = coefs_pm
        self.coefs_pm_history = [coefs_pm, ]
        self.errs_pm = []

        self.plot = plot

    def train(self, tx_dpd, rx_received):
        """Give new training data to the model"""
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

        tx_choice, rx_choice = self._sample_uniformly(tx_dpd, rx_received)
        new_coefs_am = self._next_am_coefficent(tx_choice, rx_choice)
        new_coefs_pm, phase_diff_choice = self._next_pm_coefficent(tx_choice, rx_choice)

        logging.debug('txframe: min {:.2f}, max {:.2f}, ' \
                      'median {:.2f}; rxframe: min {:.2f}, max {:.2f}, ' \
                      'median {:.2f}; new coefs_am {};' \
                      'new_coefs_pm {}'.format(
            np.min(np.abs(tx_dpd)),
            np.max(np.abs(tx_dpd)),
            np.median(np.abs(tx_dpd)),
            np.min(np.abs(rx_choice)),
            np.max(np.abs(rx_choice)),
            np.median(np.abs(rx_choice)),
            new_coefs_am,
            new_coefs_pm))

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            off = self.SA.calc_offset(tx_dpd)
            tx_mer = self.MER.calc_mer(tx_dpd[off:off + self.c.T_U])
            rx_mer = self.MER.calc_mer(rx_received[off:off + self.c.T_U], debug=True)
            self.tx_mers.append(tx_mer)
            self.rx_mers.append(rx_mer)

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_Model.svg"

            fig = plt.figure(figsize=(2 * 6, 2 * 6))

            i_sub = 1

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            ax.plot(np.abs(tx_dpd[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.abs(rx_received[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals of Iteration {}"
                         .format(len(self.coefs_am_history)))
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")
            ax.text(0, 0, "TX (max {:01.3f}, mean {:01.3f}, " \
                          "median {:01.3f})".format(
                np.max(np.abs(tx_dpd)),
                np.mean(np.abs(tx_dpd)),
                np.median(np.abs(tx_dpd))
            ), size=8)
            ax.legend(loc=4)

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            ccdf_min, ccdf_max = 0, 1
            tx_hist, ccdf_edges = np.histogram(np.abs(tx_dpd),
                                               bins=60,
                                               range=(ccdf_min, ccdf_max))
            tx_hist_normalized = tx_hist.astype(float) / np.sum(tx_hist)
            ccdf = 1.0 - np.cumsum(tx_hist_normalized)
            ax.semilogy(ccdf_edges[:-1], ccdf, label="CCDF")
            ax.semilogy(ccdf_edges[:-1],
                        tx_hist_normalized,
                        label="Histogram",
                        drawstyle='steps')
            ax.legend(loc=4)
            ax.set_ylim(1e-5, 2)
            ax.set_title("Complementary Cumulative Distribution Function")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Ratio of Samples larger than x")

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            ax.semilogy(np.array(self.mses_am) + 1e-10, label="log(MSE)")
            ax.semilogy(np.array(self.errs_am) + 1e-10, label="log(ERR)")
            ax.legend(loc=4)
            ax.set_title("MSE History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("MSE")

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            ax.plot(self.tx_mers, label="TX MER")
            ax.plot(self.rx_mers, label="RX MER")
            ax.legend(loc=4)
            ax.set_title("MER History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("MER")

            ax = plt.subplot(4, 2, i_sub)
            rx_range = np.linspace(0, 1, num=100, dtype=np.complex64)
            rx_range_dpd = self._dpd_amplitude(rx_range)[0]
            rx_range_dpd_new = self._dpd_amplitude(rx_range, new_coefs_am)[0]
            i_sub += 1
            ax.scatter(
                np.abs(tx_choice),
                np.abs(rx_choice),
                s=0.1)
            ax.plot(rx_range_dpd / self.coefs_am[0], rx_range, linewidth=0.25, label="current")
            ax.plot(rx_range_dpd_new / self.coefs_am[0], rx_range, linewidth=0.25, label="next")
            ax.set_ylim(0, 1)
            ax.set_xlim(0, 1)
            ax.legend()
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            coefs_am_history = np.array(self.coefs_am_history)
            for idx, coef_hist in enumerate(coefs_am_history.T):
                ax.plot(coef_hist,
                        label="Coef {}".format(idx),
                        linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("AM/AM Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            phase_range = np.linspace(0, 1, num=100, dtype=np.complex64)
            phase_range_dpd = self._dpd_phase(phase_range)[0]
            phase_range_dpd_new = self._dpd_phase(phase_range,
                                                 coefs=new_coefs_pm)[0]
            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            ax.scatter(
                np.abs(tx_choice),
                np.rad2deg(phase_diff_choice),
                s=0.1)
            ax.plot(
                np.abs(phase_range),
                np.rad2deg(phase_range_dpd),
                linewidth=0.25,
                label="current")
            ax.plot(
                np.abs(phase_range),
                np.rad2deg(phase_range_dpd_new),
                linewidth=0.25,
                label="next")
            ax.set_ylim(-60, 60)
            ax.set_xlim(0, 1)
            ax.legend()
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase Difference")

            ax = plt.subplot(4, 2, i_sub)
            i_sub += 1
            coefs_pm_history = np.array(self.coefs_pm_history)
            for idx, coef_phase_hist in enumerate(coefs_pm_history.T):
                ax.plot(coef_phase_hist,
                        label="Coef {}".format(idx),
                        linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("AM/PM Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            fig.tight_layout()
            fig.savefig(fig_path)
            fig.clf()

        self.coefs_am = new_coefs_am
        self.coefs_am_history.append(self.coefs_am)
        self.coefs_pm = new_coefs_pm
        self.coefs_pm_history.append(self.coefs_pm)

    def get_dpd_data(self):
        return "poly", self.coefs_am, self.coefs_pm

    def _sample_uniformly(self, tx_dpd, rx_received, n_bins=5):
        """This function returns tx and rx samples in a way
        that the tx amplitudes have an approximate uniform 
        distribution with respect to the tx_dpd amplitudes"""
        mask = np.logical_and((np.abs(tx_dpd) > 0.01), (np.abs(rx_received) > 0.01))
        tx_dpd = tx_dpd[mask]
        rx_received = rx_received[mask]

        txframe_aligned_abs = np.abs(tx_dpd)
        ccdf_min = 0
        ccdf_max = np.max(txframe_aligned_abs)
        tx_hist, ccdf_edges = np.histogram(txframe_aligned_abs,
                                           bins=n_bins,
                                           range=(ccdf_min, ccdf_max))
        n_choise = np.min(tx_hist)
        tx_choice = np.zeros(n_choise * n_bins, dtype=np.complex64)
        rx_choice = np.zeros(n_choise * n_bins, dtype=np.complex64)

        for idx, bin in enumerate(tx_hist):
            indices = np.where((txframe_aligned_abs >= ccdf_edges[idx]) &
                               (txframe_aligned_abs <= ccdf_edges[idx + 1]))[0]
            indices_choise = np.random.choice(indices,
                                              n_choise,
                                              replace=False)
            rx_choice[idx * n_choise:(idx + 1) * n_choise] = \
                rx_received[indices_choise]
            tx_choice[idx * n_choise:(idx + 1) * n_choise] = \
                tx_dpd[indices_choise]

        assert isinstance(rx_choice[0], np.complex64), \
            "rx_choice is not complex64 but {}".format(rx_choice[0].dtype)
        assert isinstance(tx_choice[0], np.complex64), \
            "tx_choice is not complex64 but {}".format(tx_choice[0].dtype)

        return tx_choice, rx_choice

    def _dpd_amplitude(self, sig, coefs=None):
        if coefs is None:
            coefs = self.coefs_am
        assert isinstance(sig[0], np.complex64), "Sig is not complex64 but {}".format(sig[0].dtype)
        sig_abs = np.abs(sig)
        A_sig = np.vstack([np.ones(sig_abs.shape),
                           sig_abs ** 1,
                           sig_abs ** 2,
                           sig_abs ** 3,
                           sig_abs ** 4,
                           ]).T
        sig_dpd = sig * np.sum(A_sig * coefs, axis=1)
        return sig_dpd, A_sig

    def _dpd_phase(self, sig, coefs=None):
        if coefs is None:
            coefs = self.coefs_pm
        assert isinstance(sig[0], np.complex64), "Sig is not complex64 but {}".format(sig[0].dtype)
        sig_abs = np.abs(sig)
        A_phase = np.vstack([np.ones(sig_abs.shape),
                             sig_abs ** 1,
                             sig_abs ** 2,
                             sig_abs ** 3,
                             sig_abs ** 4,
                             ]).T
        phase_diff_est = np.sum(A_phase * coefs, axis=1)
        return phase_diff_est, A_phase

    def _next_am_coefficent(self, tx_choice, rx_choice):
        """Calculate new coefficients for AM/AM correction"""
        rx_dpd, rx_A = self._dpd_amplitude(rx_choice)
        rx_dpd = rx_dpd * (
            np.median(np.abs(tx_choice)) /
            np.median(np.abs(rx_dpd)))
        err = np.abs(rx_dpd) - np.abs(tx_choice)
        mse = np.mean(np.abs((rx_dpd - tx_choice) ** 2))
        self.mses_am.append(mse)
        self.errs_am.append(np.mean(err**2))

        reg = linear_model.Ridge(alpha=0.00001)
        reg.fit(rx_A, err)
        a_delta = reg.coef_
        new_coefs_am = self.coefs_am - self.learning_rate_am * a_delta
        new_coefs_am = new_coefs_am * (self.coefs_am[0] / new_coefs_am[0])
        return new_coefs_am

    def _next_pm_coefficent(self, tx_choice, rx_choice):
        """Calculate new coefficients for AM/PM correction
        Assuming deviations smaller than pi/2"""
        phase_diff_choice = np.angle(
            (rx_choice * tx_choice.conjugate()) /
            (np.abs(rx_choice) * np.abs(tx_choice))
        )
        plt.hist(phase_diff_choice)
        plt.savefig('/tmp/hist_' + str(np.random.randint(0,1000)) + '.svg')
        plt.clf()
        phase_diff_est, phase_A = self._dpd_phase(rx_choice)
        err_phase = phase_diff_est - phase_diff_choice
        self.errs_pm.append(np.mean(np.abs(err_phase ** 2)))

        reg = linear_model.Ridge(alpha=0.00001)
        reg.fit(phase_A, err_phase)
        p_delta = reg.coef_
        new_coefs_pm = self.coefs_pm - self.learning_rate_pm * p_delta

        return new_coefs_pm, phase_diff_choice

class LutModel:
    """Implements a model that calculates lookup table coefficients"""

    def __init__(self,
                 c,
                 SA,
                 MER,
                 learning_rate=1.,
                 plot=False):
        logging.debug("Initialising LUT Model")
        self.c = c
        self.SA = SA
        self.MER = MER
        self.learning_rate = learning_rate
        self.plot = plot

    def train(self, tx_dpd, rx_received):
        pass

    def get_dpd_data(self):
        return ("lut", np.ones(32, dtype=np.complex64))

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
