# -*- coding: utf-8 -*-

import numpy as np
import datetime
import logging
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt

class Model:
    """Calculates new coefficients using the measurement and the old
    coefficients"""

    def __init__(self, coefs):
        self.coefs = coefs

    def get_next_coefs(self, txframe_aligned, rxframe_aligned):
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

            tx_rx_frame_path = ('/tmp/tx_rx_sync_' +
                            datetime.datetime.now().isoformat() +
                            '.pdf')
            plt.plot(np.abs(rxframe_aligned[:128]), label="rxframe")
            plt.plot(np.abs(txframe_aligned[:128]), label="txframe")
            plt.xlabel("Samples")
            plt.ylabel("Real Part")
            plt.legend()
            plt.savefig(tx_rx_frame_path)
            plt.clf()
            logging.debug("Tx, Rx synchronized %s" % tx_rx_frame_path)

            dt = datetime.datetime.now().isoformat()
            txframe_path = ('/tmp/tx_3_' + dt + '.iq')
            txframe_aligned.tofile(txframe_path)
            rxframe_path = ('/tmp/rx_3_' + dt + '.iq')
            rxframe_aligned.tofile(rxframe_path)

        mse = np.mean(np.abs(np.square(txframe_aligned - rxframe_aligned)))
        logging.debug("MSE: {}".format(mse))

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
