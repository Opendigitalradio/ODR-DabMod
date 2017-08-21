# -*- coding: utf-8 -*-

import numpy as np
import datetime
import logging
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
from sklearn.linear_model import Ridge

class Model:
    """Calculates new coefficients using the measurement and the old
    coefficients"""

    def __init__(self, coefs):
        self.coefs = coefs

    def get_next_coefs(self, txframe_aligned, rxframe_aligned):
        rx_abs = np.abs(rxframe_aligned)
        A = np.vstack([rx_abs,
                       rx_abs**3,
                       rx_abs**5,
                       rx_abs**7,
                       rx_abs**9,
                       ]).T
        y = np.abs(txframe_aligned)

        clf = Ridge(alpha=10)
        clf.fit(A, y)
        sol = clf.coef_

        rx_range = np.linspace(0,1,50)
        A_range = np.vstack([
                       rx_range,
                       rx_range**3,
                       rx_range**5,
                       rx_range**7,
                       rx_range**9,
                       ]).T
        y_est = np.sum(A_range * sol, axis=1)

        logging.debug("New coefficents {}".format(sol))

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
            fig_path = "/tmp/" + dt + "_Model.pdf"

            fig, axs = plt.subplots(5, figsize=(6,2*6))

            ax = axs[0]
            ax.plot(np.abs(txframe_aligned[:128]), label="TX Frame")
            ax.plot(np.abs(rxframe_aligned[:128]), label="RX Frame")
            ax.set_title("Synchronized Signals")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Amplitude")
            ax.legend(loc=4)

            ax = axs[1]
            ax.plot(np.real(txframe_aligned[:128]), label="TX Frame")
            ax.plot(np.real(rxframe_aligned[:128]), label="RX Frame")
            ax.set_title("Synchronized Signals")
            ax.set_xlabel("Samples")
            ax.set_ylabel("Real Part")
            ax.legend(loc=4)

            ax = axs[2]
            ax.scatter(
                np.abs(txframe_aligned[:1024]),
                np.abs(rxframe_aligned[:1024]),
                s = 0.1
            )
            ax.plot(
                y_est,
                rx_range,
                linewidth=0.25
            )
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

            fig.tight_layout()
            fig.savefig(fig_path)
            fig.clf()

        mse = np.mean(np.abs(np.square(txframe_aligned[:1024] - rxframe_aligned[:1024])))
        logging.debug("MSE: {}".format(mse))

        sol = sol * 1.7/sol[0]
        return sol

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
