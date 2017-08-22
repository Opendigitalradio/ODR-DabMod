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

    def __init__(self, coefs):
        self.coefs = coefs
        self.coefs_history = [coefs,]
        self.mses = [0,]
        self.errs = [0,]

    def get_next_coefs(self, txframe_aligned, rxframe_aligned):
        rx_abs = np.abs(rxframe_aligned)
        rx_A = np.vstack([rx_abs,
                       rx_abs**3,
                       rx_abs**5,
                       rx_abs**7,
                       rx_abs**9,
                       ]).T
        rx_dpd = np.sum(rx_A * self.coefs, axis=1)
        rx_dpd = rx_dpd * (
            np.median(np.abs(txframe_aligned)) / np.median(np.abs(rx_dpd)))

        err = rx_dpd - np.abs(txframe_aligned)
        self.errs.append(np.mean(np.abs(err**2)))

        a_delta = np.linalg.lstsq(rx_A, err)[0]
        new_coefs = self.coefs - 0.1 * a_delta
        logging.debug("a_delta {}".format(a_delta))
        logging.debug("new coefs {}".format(new_coefs))

        tx_abs = np.abs(rxframe_aligned)
        tx_A = np.vstack([tx_abs,
                          tx_abs**3,
                          tx_abs**5,
                          tx_abs**7,
                          tx_abs**9,
                          ]).T
        tx_dpd = np.sum(tx_A * new_coefs, axis=1)

        tx_dpd_norm = tx_dpd * (
            np.median(np.abs(txframe_aligned)) / np.median(np.abs(tx_dpd)))

        rx_A_complex = np.vstack([rxframe_aligned,
                          rxframe_aligned * rx_abs**2,
                          rxframe_aligned * rx_abs**4,
                          rxframe_aligned * rx_abs**6,
                          rxframe_aligned * rx_abs**8,
                          ]).T
        rx_post_distored = np.sum(rx_A_complex * self.coefs, axis=1)
        rx_post_distored = rx_post_distored * (
            np.median(np.abs(txframe_aligned)) /
            np.median(np.abs(rx_post_distored)))
        mse = np.mean(np.abs((txframe_aligned - rx_post_distored)**2))
        logging.debug("MSE: {}".format(mse))
        self.mses.append(mse)

        def dpd(tx):
            tx_abs = np.abs(tx)
            tx_A_complex = np.vstack([tx,
                                  tx * tx_abs**2,
                                  tx * tx_abs**4,
                                  tx * tx_abs**6,
                                  tx * tx_abs**8,
                                  ]).T
            tx_dpd = np.sum(tx_A_complex * self.coefs, axis=1)
            return tx_dpd
        tx_inverse_dpd = inversefunc(dpd, y_values=txframe_aligned[:128])
        tx_inverse_dpd = tx_inverse_dpd * (
            np.median(np.abs(txframe_aligned)) /
            np.median(np.abs(tx_inverse_dpd))
        )

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            logging.debug("txframe: min %f, max %f, median %f" %
                          (np.min(np.abs(txframe_aligned)),
                           np.max(np.abs(txframe_aligned)),
                           np.median(np.abs(txframe_aligned))
                           ))

            logging.debug("rxframe: min %f, max %f, median %f" %
                          (np.min(np.abs(rxframe_aligned)),
                           np.max(np.abs(rxframe_aligned)),
                           np.median(np.abs(rxframe_aligned))
                           ))

            dt = datetime.datetime.now().isoformat()
            fig_path = logging_path + "/" + dt + "_Model.pdf"

            fig, axs = plt.subplots(7, figsize=(6,3*6))

            ax = axs[0]
            ax.plot(np.abs(txframe_aligned[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.abs(tx_inverse_dpd[:128]),
                    label="TX inverse dpd",
                    color="green")
            ax.plot(np.abs(rxframe_aligned[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals of Iteration {}".format(len(self.coefs_history)))
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")
            ax.legend(loc=4)

            ax = axs[1]
            ax.plot(np.real(txframe_aligned[:128]),
                    label="TX sent",
                    linestyle=":")
            ax.plot(np.real(tx_inverse_dpd[:128]),
                    label="TX inverse dpd",
                    color="green")
            ax.plot(np.real(rxframe_aligned[:128]),
                    label="RX received",
                    color="red")
            ax.set_title("Synchronized Signals")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Real Part")
            ax.legend(loc=4)

            ax = axs[2]
            ax.scatter(
                np.abs(txframe_aligned[:1024]),
                np.abs(rxframe_aligned[:1024]),
                s = 0.1)
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("RX Amplitude")

            ax = axs[3]
            angle_diff_rad = ((
                             (np.angle(txframe_aligned[:1024]) -
                              np.angle(rxframe_aligned[:1024]) +
                              np.pi) % (2 * np.pi)) -
                          np.pi
                          )
            ax.scatter(
                np.abs(txframe_aligned[:1024]),
                angle_diff_rad * 180 / np.pi,
                s = 0.1
            )
            ax.set_title("Amplifier Characteristic")
            ax.set_xlabel("TX Amplitude")
            ax.set_ylabel("Phase Difference [deg]")

            ax = axs[4]
            ax.plot(np.abs(txframe_aligned[:128]),
                     label="TX Frame",
                     linestyle=":",
                     linewidth=0.5)
            ax.plot(np.abs(rxframe_aligned[:128]),
                     label="RX Frame",
                     linestyle="--",
                     linewidth=0.5)
            ax.plot(np.abs(rx_dpd[:128]),
                     label="RX DPD Frame",
                     linestyle="-.",
                     linewidth=0.5)
            ax.plot(np.abs(tx_dpd_norm[:128]),
                     label="TX DPD Frame Norm",
                     linestyle="-.",
                     linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("RX DPD")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")

            ax = axs[5]
            coefs_history = np.array(self.coefs_history)
            for idx, coef_hist in enumerate(coefs_history.T):
                ax.plot(coef_hist,
                     label="Coef {}".format(idx),
                     linewidth=0.5)
            ax.legend(loc=4)
            ax.set_title("Coefficient History")
            ax.set_xlabel("Iterations")
            ax.set_ylabel("Coefficient Value")

            ax = axs[6]
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

        self.coefs = new_coefs
        self.coefs_history.append(self.coefs)
        return self.coefs

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
