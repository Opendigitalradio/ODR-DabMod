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

class Model:
    """Calculates new coefficients using the measurement and the old
    coefficients"""

    def __init__(self, c, SA, MER, coefs_am, coefs_pm, plot=False):
        self.c = c
        self.SA = SA
        self.MER = MER

        self.coefs_am = coefs_am
        self.coefs_history = [coefs_am, ]
        self.mses = []
        self.errs = []
        self.tx_mers = []
        self.rx_mers = []

        self.coefs_pm = coefs_pm
        self.coefs_pm_history = [coefs_pm, ]
        self.errs_phase = [0, ]

        self.plot=plot

    def sample_uniformly(self, tx_dpd, rx_received, n_bins=4):
        """This function returns tx and rx samples in a way
        that the tx amplitudes have an approximate uniform 
        distribution with respect to the tx_dpd amplitudes"""
        txframe_aligned_abs = np.abs(tx_dpd)
        ccdf_min = 0
        ccdf_max = np.max(txframe_aligned_abs)
        tx_hist, ccdf_edges = np.histogram(txframe_aligned_abs,
                                           bins=n_bins,
                                           range=(ccdf_min, ccdf_max))
        n_choise  = np.min(tx_hist)
        tx_choice = np.zeros(n_choise * n_bins, dtype=np.complex64)
        rx_choice = np.zeros(n_choise * n_bins, dtype=np.complex64)

        for idx, bin in enumerate(tx_hist):
            indices = np.where((txframe_aligned_abs >= ccdf_edges[idx]) &
                               (txframe_aligned_abs <= ccdf_edges[idx+1]))[0]
            indices_choise = np.random.choice(indices, n_choise, replace=False)
            rx_choice[idx*n_choise:(idx+1)*n_choise] = rx_received[indices_choise]
            tx_choice[idx*n_choise:(idx+1)*n_choise] = tx_dpd[indices_choise]
        return tx_choice, rx_choice

    def amplitude_predistortion(self, sig):
        sig_abs = np.abs(sig)
        A_sig = np.vstack([np.ones(sig_abs.shape),
                          sig_abs ** 1,
                          sig_abs ** 2,
                          sig_abs ** 3,
                          sig_abs ** 4,
                          ]).T
        sig_dpd = sig * np.sum(A_sig * self.coefs_am, axis=1)
        return sig_dpd, A_sig

    def dpd_phase(self, tx):
        tx_abs = np.abs(tx)
        tx_A_complex = np.vstack([tx,
                                  tx * tx_abs ** 1,
                                  tx * tx_abs ** 2,
                                  tx * tx_abs ** 3,
                                  tx * tx_abs ** 4,
                                  ]).T
        tx_dpd = np.sum(tx_A_complex * self.coefs_pm, axis=1)
        return tx_dpd

    def get_next_coefs(self, tx_dpd, rx_received):
        normalization_error = np.abs(np.median(np.abs(tx_dpd)) - np.median(np.abs(rx_received)))/(np.median(np.abs(tx_dpd)) + np.median(np.abs(rx_received)))
        assert normalization_error < 0.01, "Non normalized signals"
        tx_choice, rx_choice = self.sample_uniformly(tx_dpd, rx_received)

        # Calculate new coefficients for AM/AM correction
        rx_dpd, rx_A = self.amplitude_predistortion(rx_choice)
        rx_dpd = rx_dpd * (
            np.median(np.abs(tx_choice)) /
            np.median(np.abs(rx_dpd)))

        err = np.abs(rx_dpd) - np.abs(tx_choice)
        self.errs.append(np.mean(np.abs(err ** 2)))

        mse = np.mean(np.abs((rx_dpd - tx_choice)**2))
        self.mses.append(mse)

        a_delta = np.linalg.lstsq(rx_A, err)[0]
        new_coefs = self.coefs_am - 0.1 * a_delta
        new_coefs = new_coefs * (self.coefs_am[0] / new_coefs[0])
        assert np.abs(self.coefs_am[0] / new_coefs[0] - 1) < 0.1, \
            "Too large change in first " \
            "coefficient. {}, {}".format(self.coefs_am[0], new_coefs[0])
        logging.debug("a_delta {}".format(a_delta))
        logging.debug("new coefs_am {}".format(new_coefs))

        rx_range = np.linspace(0, 1, num=100)
        rx_range_dpd = self.amplitude_predistortion(rx_range)[0]
        rx_range = rx_range[(rx_range_dpd > 0) & (rx_range_dpd < 2)]
        rx_range_dpd = rx_range_dpd[(rx_range_dpd > 0) & (rx_range_dpd < 2)]

        logging.debug('txframe: min {:.2f}, max {:.2f}, ' \
                      'median {:.2f}; rxframe: min {:.2f}, max {:.2f}, ' \
                      'median {:.2f}; a_delta {}; new coefs_am {}'.format(
            np.min(np.abs(tx_dpd)),
            np.max(np.abs(tx_dpd)),
            np.median(np.abs(tx_dpd)),
            np.min(np.abs(rx_choice)),
            np.max(np.abs(rx_choice)),
            np.median(np.abs(rx_choice)),
            a_delta,
            new_coefs))

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            off = self.SA.calc_offset(tx_dpd)
            tx_mer = self.MER.calc_mer(tx_dpd[off:off + self.c.T_U])
            rx_mer = self.MER.calc_mer(rx_received[off:off + self.c.T_U], debug=True)
            self.tx_mers.append(tx_mer)
            self.rx_mers.append(rx_mer)

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG and self.plot:
            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_Model.svg"

            fig = plt.figure(figsize=(3*6, 6))

            ax = plt.subplot(2,3,1)
            ax.plot(np.abs(tx_dpd[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.abs(rx_received[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals of Iteration {}".format(len(self.coefs_history)))
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")
            ax.text(0, 0, "TX (max {:01.3f}, mean {:01.3f}, median {:01.3f})".format(
                np.max(np.abs(tx_dpd)),
                np.mean(np.abs(tx_dpd)),
                np.median(np.abs(tx_dpd))
            ), size = 8)
            ax.legend(loc=4)

            ax = plt.subplot(2,3,2)
            ax.scatter(
                np.abs(tx_choice),
                np.abs(rx_choice),
                s=0.1)
            ax.plot(rx_range_dpd / self.coefs_am[0], rx_range, linewidth=0.25)
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")

            ax = plt.subplot(2,3,3)
            ccdf_min, ccdf_max = 0, 1
            tx_hist, ccdf_edges = np.histogram(np.abs(tx_dpd),
                                      bins=60,
                                      range=(ccdf_min, ccdf_max))
            tx_hist_normalized = tx_hist.astype(float)/np.sum(tx_hist)
            ccdf = 1.0 - np.cumsum(tx_hist_normalized)
            ax.semilogy(ccdf_edges[:-1], ccdf, label="CCDF")
            ax.semilogy(ccdf_edges[:-1],
                        tx_hist_normalized,
                        label="Histogram",
                        drawstyle='steps')
            ax.legend(loc=4)
            ax.set_ylim(1e-5,2)
            ax.set_title("Complementary Cumulative Distribution Function")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Ratio of Samples larger than x")

            ax = plt.subplot(2,3,4)
            coefs_history = np.array(self.coefs_history)
            for idx, coef_hist in enumerate(coefs_history.T):
                ax.plot(coef_hist,
                        label="Coef {}".format(idx),
                        linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("AM/AM Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            ax = plt.subplot(2,3,5)
            ax.plot(self.tx_mers, label="TX MER")
            ax.plot(self.rx_mers, label="RX MER")
            ax.legend(loc=4)
            ax.set_title("MER History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("MER")

            ax = plt.subplot(2,3,6)
            ax.plot(self.mses, label="MSE")
            ax.plot(self.errs, label="ERR")
            ax.legend(loc=4)
            ax.set_title("MSE History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("MSE")

            fig.tight_layout()
            fig.savefig(fig_path)
            fig.clf()

        self.coefs_am = new_coefs
        self.coefs_history.append(self.coefs_am)
        return self.coefs_am, self.coefs_pm

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
