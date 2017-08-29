# -*- coding: utf-8 -*-

import datetime
import os
import logging
logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)

from pynverse import inversefunc
import numpy as np
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
from sklearn.linear_model import Ridge

class Model:
    """Calculates new coefficients using the measurement and the old
    coefficients"""

    def __init__(self, coefs_am, coefs_pm):
        self.coefs_am = coefs_am
        self.coefs_history = [coefs_am, ]
        self.mses = [0, ]
        self.errs = [0, ]

        self.coefs_pm = coefs_pm
        self.coefs_pm_history = [coefs_pm, ]
        self.errs_phase = [0, ]

    def sample_uniformly(self, txframe_aligned, rxframe_aligned, n_bins=4):
        """This function returns tx and rx samples in a way
        that the tx amplitudes have an approximate uniform 
        distribution with respect to the txframe_aligned amplitudes"""
        txframe_aligned_abs = np.abs(txframe_aligned)
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
            rx_choice[idx*n_choise:(idx+1)*n_choise] = rxframe_aligned[indices_choise]
            tx_choice[idx*n_choise:(idx+1)*n_choise] = txframe_aligned[indices_choise]
        return tx_choice, rx_choice

    def get_next_coefs(self, txframe_aligned, rxframe_aligned):
        tx_choice, rx_choice = self.sample_uniformly(txframe_aligned, rxframe_aligned)

        # Calculate new coefficients for AM/AM correction
        rx_abs = np.abs(rx_choice)
        rx_A = np.vstack([rx_abs,
                          rx_abs ** 3,
                          rx_abs ** 5,
                          rx_abs ** 7,
                          rx_abs ** 9,
                          ]).T
        rx_dpd = np.sum(rx_A * self.coefs_am, axis=1)
        rx_dpd = rx_dpd * (
            np.median(np.abs(tx_choice)) / np.median(np.abs(rx_dpd)))

        err = rx_dpd - np.abs(tx_choice)
        self.errs.append(np.mean(np.abs(err ** 2)))

        a_delta = np.linalg.lstsq(rx_A, err)[0]
        new_coefs = self.coefs_am - 0.1 * a_delta
        new_coefs = new_coefs * (self.coefs_am[0] / new_coefs[0])
        logging.debug("a_delta {}".format(a_delta))
        logging.debug("new coefs_am {}".format(new_coefs))

        # Calculate new coefficients for AM/PM correction
        phase_diff_rad = ((
                              (np.angle(tx_choice) -
                               np.angle(rx_choice) +
                               np.pi) % (2 * np.pi)) -
                          np.pi
                          )

        tx_abs = np.abs(tx_choice)
        tx_abs_A = np.vstack([tx_abs,
                             tx_abs ** 2,
                             tx_abs ** 3,
                             tx_abs ** 4,
                             tx_abs ** 5,
                             ]).T
        phase_dpd = np.sum(tx_abs_A * self.coefs_pm, axis=1)

        err_phase = phase_dpd - phase_diff_rad
        self.errs_phase.append(np.mean(np.abs(err_phase ** 2)))
        a_delta = np.linalg.lstsq(tx_abs_A, err_phase)[0]
        new_coefs_pm = self.coefs_pm - 0.1 * a_delta
        logging.debug("a_delta {}".format(a_delta))
        logging.debug("new new_coefs_pm {}".format(new_coefs_pm))

        def dpd_phase(tx):
            tx_abs = np.abs(tx)
            tx_A_complex = np.vstack([tx,
                                      tx * tx_abs ** 1,
                                      tx * tx_abs ** 2,
                                      tx * tx_abs ** 3,
                                      tx * tx_abs ** 4,
                                      ]).T
            tx_dpd = np.sum(tx_A_complex * self.coefs_pm, axis=1)
            return tx_dpd

        tx_range = np.linspace(0, 2)
        phase_range_dpd = dpd_phase(tx_range)

        rx_A_complex = np.vstack([rx_choice,
                                  rx_choice * rx_abs ** 2,
                                  rx_choice * rx_abs ** 4,
                                  rx_choice * rx_abs ** 6,
                                  rx_choice * rx_abs ** 8,
                                  ]).T
        rx_post_distored = np.sum(rx_A_complex * self.coefs_am, axis=1)
        rx_post_distored = rx_post_distored * (
            np.median(np.abs(tx_choice)) /
            np.median(np.abs(rx_post_distored)))
        mse = np.mean(np.abs((tx_choice - rx_post_distored) ** 2))
        logging.debug("MSE: {}".format(mse))
        self.mses.append(mse)

        def dpd(tx):
            tx_abs = np.abs(tx)
            tx_A_complex = np.vstack([tx,
                                      tx * tx_abs ** 2,
                                      tx * tx_abs ** 4,
                                      tx * tx_abs ** 6,
                                      tx * tx_abs ** 8,
                                      ]).T
            tx_dpd = np.sum(tx_A_complex * self.coefs_am, axis=1)
            return tx_dpd

        rx_range = np.linspace(0, 1, num=100)
        rx_range_dpd = dpd(rx_range)
        rx_range = rx_range[(rx_range_dpd > 0) & (rx_range_dpd < 2)]
        rx_range_dpd = rx_range_dpd[(rx_range_dpd > 0) & (rx_range_dpd < 2)]

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            logging.debug("txframe: min %f, max %f, median %f" %
                          (np.min(np.abs(txframe_aligned)),
                           np.max(np.abs(txframe_aligned)),
                           np.median(np.abs(txframe_aligned))
                           ))

            logging.debug("rxframe: min %f, max %f, median %f" %
                          (np.min(np.abs(rx_choice)),
                           np.max(np.abs(rx_choice)),
                           np.median(np.abs(rx_choice))
                           ))

            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_Model.pdf"

            fig = plt.figure(figsize=(3*6, 1.5 * 6))

            ax = plt.subplot(3,3,1)
            ax.plot(np.abs(txframe_aligned[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.abs(rxframe_aligned[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals of Iteration {}".format(len(self.coefs_history)))
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")
            ax.text(0, 0, "TX (max {:01.3f}, mean {:01.3f}, median {:01.3f})".format(
                np.max(np.abs(txframe_aligned)),
                np.mean(np.abs(txframe_aligned)),
                np.median(np.abs(txframe_aligned))
            ), size = 8)
            ax.legend(loc=4)

            ax = plt.subplot(3,3,2)
            ax.plot(np.real(txframe_aligned[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.real(rxframe_aligned[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Real Part")
            ax.legend(loc=4)

            ax = plt.subplot(3,3,3)
            ax.plot(np.abs(txframe_aligned[:128]),
                    label="TX Frame",
                    linestyle=":",
                    linewidth=0.5)
            ax.plot(np.abs(rxframe_aligned[:128]),
                    label="RX Frame",
                    linestyle="--",
                    linewidth=0.5)

            rx_abs = np.abs(rxframe_aligned)
            rx_A = np.vstack([rx_abs,
                              rx_abs ** 3,
                              rx_abs ** 5,
                              rx_abs ** 7,
                              rx_abs ** 9,
                              ]).T
            rx_dpd = np.sum(rx_A * self.coefs_am, axis=1)
            rx_dpd = rx_dpd * (
                np.median(np.abs(tx_choice)) / np.median(np.abs(rx_dpd)))

            ax.plot(np.abs(rx_dpd[:128]),
                    label="RX DPD Frame",
                    linestyle="-.",
                    linewidth=0.5)

            tx_abs = np.abs(np.abs(txframe_aligned[:128]))
            tx_A = np.vstack([tx_abs,
                              tx_abs ** 3,
                              tx_abs ** 5,
                              tx_abs ** 7,
                              tx_abs ** 9,
                              ]).T
            tx_dpd = np.sum(tx_A * new_coefs, axis=1)
            tx_dpd_norm = tx_dpd * (
                np.median(np.abs(tx_choice)) / np.median(np.abs(tx_dpd)))

            ax.plot(np.abs(tx_dpd_norm[:128]),
                    label="TX DPD Frame Norm",
                    linestyle="-.",
                    linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("RX DPD")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")

            ax = plt.subplot(3,3,4)
            ax.scatter(
                np.abs(tx_choice[:1024]),
                np.abs(rx_choice[:1024]),
                s=0.1)
            ax.plot(rx_range_dpd / self.coefs_am[0], rx_range, linewidth=0.25)
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")

            ax = plt.subplot(3,3,5)
            ax.scatter(
                np.abs(tx_choice[:1024]),
                phase_diff_rad[:1024] * 180 / np.pi,
                s=0.1
            )
            ax.plot(tx_range, phase_range_dpd * 180 / np.pi, linewidth=0.25)
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase Difference [deg]")

            ax = plt.subplot(3,3,6)
            ccdf_min, ccdf_max = 0, 1
            tx_hist, ccdf_edges = np.histogram(np.abs(txframe_aligned),
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

            ax = plt.subplot(3,3,7)
            coefs_history = np.array(self.coefs_history)
            for idx, coef_hist in enumerate(coefs_history.T):
                ax.plot(coef_hist,
                        label="Coef {}".format(idx),
                        linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("AM/AM Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            ax = plt.subplot(3,3,8)
            coefs_history = np.array(self.coefs_pm_history)
            for idx, coef_hist in enumerate(coefs_history.T):
                ax.plot(coef_hist,
                        label="Coef {}".format(idx),
                        linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("AM/PM Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            ax = plt.subplot(3,3,9)
            coefs_history = np.array(self.coefs_history)
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
        self.coefs_pm = new_coefs_pm
        self.coefs_pm_history.append(self.coefs_pm)
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
