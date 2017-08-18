# -*- coding: utf-8 -*-

import numpy as np
import scipy
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
import datetime
import src.subsample_align as sa
from scipy import signal
import logging

class Dab_Util:
    """Collection of methods that can be applied to an array
     complex IQ samples of a DAB signal
     """
    def __init__(self, sample_rate):
        """
        :param sample_rate: sample rate [sample/sec] to use for calculations
        """
        self.sample_rate = sample_rate
        self.dab_bandwidth = 1536000 #Bandwidth of a dab signal
        self.frame_ms = 96           #Duration of a Dab frame

    def lag(self, sig_orig, sig_rec):
        """
        Find lag between two signals
        Args:
            sig_orig: The signal that has been sent
            sig_rec: The signal that has been recored
        """
        off = sig_rec.shape[0]
        c = np.abs(signal.correlate(sig_orig, sig_rec))

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            corr_path = ('/tmp/tx_rx_corr_' +
                                datetime.datetime.now().isoformat() +
                                '.pdf')
            plt.plot(c, label="corr")
            plt.legend()
            plt.savefig(corr_path)
            plt.clf()

        return np.argmax(c) - off + 1

    def lag_upsampling(self, sig_orig, sig_rec, n_up):
        if n_up != 1:
            sig_orig_up = signal.resample(sig_orig, sig_orig.shape[0] * n_up)
            sig_rec_up  = signal.resample(sig_rec, sig_rec.shape[0] * n_up)
        else:
            sig_orig_up = sig_orig
            sig_rec_up  = sig_rec
        l = self.lag(sig_orig_up, sig_rec_up)
        l_orig = float(l) / n_up
        return l_orig

    def subsample_align_upsampling(self, sig1, sig2, n_up=32):
        """
        Returns an aligned version of sig1 and sig2 by cropping and subsample alignment
        Using upsampling
        """
        assert(sig1.shape[0] == sig2.shape[0])

        if sig1.shape[0] % 2 == 1:
            sig1 = sig1[:-1]
            sig2 = sig2[:-1]

        sig1_up = signal.resample(sig1, sig1.shape[0] * n_up)
        sig2_up = signal.resample(sig2, sig2.shape[0] * n_up)

        off_meas = self.lag_upsampling(sig2_up, sig1_up, n_up=1)
        off = int(abs(off_meas))

        if off_meas > 0:
            sig1_up = sig1_up[:-off]
            sig2_up = sig2_up[off:]
        elif off_meas < 0:
            sig1_up = sig1_up[off:]
        sig2_up = sig2_up[:-off]

        sig1 = signal.resample(sig1_up, sig1_up.shape[0] / n_up).astype(np.complex64)
        sig2 = signal.resample(sig2_up, sig2_up.shape[0] / n_up).astype(np.complex64)
        return sig1, sig2

    def subsample_align(self, sig1, sig2):
        """
        Returns an aligned version of sig1 and sig2 by cropping and subsample alignment
        """

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            dt = datetime.datetime.now().isoformat()
            txframe_path = ('/tmp/tx_0_' + dt + '.iq')
            sig1.tofile(txframe_path)
            rxframe_path = ('/tmp/rx_0_' + dt + '.iq')
            sig2.tofile(rxframe_path)

        logging.debug("Sig1_orig: %d %s, Sig2_orig: %d %s" % (len(sig1), sig1.dtype, len(sig2), sig2.dtype))
        off_meas = self.lag_upsampling(sig2, sig1, n_up=1)
        off = int(abs(off_meas))

        if off_meas > 0:
            sig1 = sig1[:-off]
            sig2 = sig2[off:]
        elif off_meas < 0:
            sig1 = sig1[off:]
            sig2 = sig2[:-off]

        if off % 2 == 1:
            sig1 = sig1[:-1]
            sig2 = sig2[:-1]

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            txframe_path = ('/tmp/tx_1_' + dt + '.iq')
            sig1.tofile(txframe_path)
            rxframe_path = ('/tmp/rx_1_' + dt + '.iq')
            sig2.tofile(rxframe_path)

        sig2 = sa.subsample_align(sig2, sig1)

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            txframe_path = ('/tmp/tx_2_' + dt + '.iq')
            sig1.tofile(txframe_path)
            rxframe_path = ('/tmp/rx_2_' + dt + '.iq')
            sig2.tofile(rxframe_path)

        logging.debug("Sig1_cut: %d %s, Sig2_cut: %d %s, off: %d" % (len(sig1), sig1.dtype, len(sig2), sig2.dtype, off))
        return sig1, sig2

    def fromfile(self, filename, offset=0, length=None):
        if length is None:
            return np.memmap(filename, dtype=np.complex64, mode='r', offset=64/8*offset)
        else:
            return np.memmap(filename, dtype=np.complex64, mode='r', offset=64/8*offset, shape=length)


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
