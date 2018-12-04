# -*- coding: utf-8 -*-
#
# DPD Computation Engine, utilities for working with DAB signals.
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
import dpd.subsample_align as sa
import dpd.phase_align as pa
from scipy import signal


def fromfile(filename, offset=0, length=None):
    if length is None:
        return np.memmap(filename, dtype=np.complex64, mode='r', offset=64 / 8 * offset)
    else:
        return np.memmap(filename, dtype=np.complex64, mode='r', offset=64 / 8 * offset, shape=length)


class Dab_Util:
    """Collection of methods that can be applied to an array
     complex IQ samples of a DAB signal
     """

    def __init__(self, config, sample_rate, plot=False):
        """
        :param sample_rate: sample rate [sample/sec] to use for calculations
        """
        self.c = config
        self.sample_rate = sample_rate
        self.dab_bandwidth = 1536000  # Bandwidth of a dab signal
        self.frame_ms = 96  # Duration of a Dab frame

        self.plot = plot

    def lag(self, sig_orig, sig_rec):
        """
        Find lag between two signals
        Args:
            sig_orig: The signal that has been sent
            sig_rec: The signal that has been recored
        """
        off = sig_rec.shape[0]
        c = np.abs(signal.correlate(sig_orig, sig_rec))

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            corr_path = self.c.plot_location + "/" + dt + "_tx_rx_corr.png"
            plt.plot(c, label="corr")
            plt.legend()
            plt.savefig(corr_path)
            plt.close()

        return np.argmax(c) - off + 1

    def lag_upsampling(self, sig_orig, sig_rec, n_up):
        if n_up != 1:
            sig_orig_up = signal.resample(sig_orig, sig_orig.shape[0] * n_up)
            sig_rec_up = signal.resample(sig_rec, sig_rec.shape[0] * n_up)
        else:
            sig_orig_up = sig_orig
            sig_rec_up = sig_rec
        l = self.lag(sig_orig_up, sig_rec_up)
        l_orig = float(l) / n_up
        return l_orig

    def subsample_align_upsampling(self, sig_tx, sig_rx, n_up=32):
        """
        Returns an aligned version of sig_tx and sig_rx by cropping and subsample alignment
        Using upsampling
        """
        assert (sig_tx.shape[0] == sig_rx.shape[0])

        if sig_tx.shape[0] % 2 == 1:
            sig_tx = sig_tx[:-1]
            sig_rx = sig_rx[:-1]

        sig1_up = signal.resample(sig_tx, sig_tx.shape[0] * n_up)
        sig2_up = signal.resample(sig_rx, sig_rx.shape[0] * n_up)

        off_meas = self.lag_upsampling(sig2_up, sig1_up, n_up=1)
        off = int(abs(off_meas))

        if off_meas > 0:
            sig1_up = sig1_up[:-off]
            sig2_up = sig2_up[off:]
        elif off_meas < 0:
            sig1_up = sig1_up[off:]
        sig2_up = sig2_up[:-off]

        sig_tx = signal.resample(sig1_up, sig1_up.shape[0] / n_up).astype(np.complex64)
        sig_rx = signal.resample(sig2_up, sig2_up.shape[0] / n_up).astype(np.complex64)
        return sig_tx, sig_rx

    def subsample_align(self, sig_tx, sig_rx):
        """
        Returns an aligned version of sig_tx and sig_rx by cropping and subsample alignment
        """

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_sync_raw.png"

            fig, axs = plt.subplots(2)
            axs[0].plot(np.abs(sig_tx[:128]), label="TX Frame")
            axs[0].plot(np.abs(sig_rx[:128]), label="RX Frame")
            axs[0].set_title("Raw Data")
            axs[0].set_ylabel("Amplitude")
            axs[0].set_xlabel("Samples")
            axs[0].legend(loc=4)

            axs[1].plot(np.real(sig_tx[:128]), label="TX Frame")
            axs[1].plot(np.real(sig_rx[:128]), label="RX Frame")
            axs[1].set_title("Raw Data")
            axs[1].set_ylabel("Real Part")
            axs[1].set_xlabel("Samples")
            axs[1].legend(loc=4)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

        off_meas = self.lag_upsampling(sig_rx, sig_tx, n_up=1)
        off = int(abs(off_meas))

        logging.debug("sig_tx_orig: {} {}, sig_rx_orig: {} {}, offset {}".format(
            len(sig_tx),
            sig_tx.dtype,
            len(sig_rx),
            sig_rx.dtype,
            off_meas))

        if off_meas > 0:
            sig_tx = sig_tx[:-off]
            sig_rx = sig_rx[off:]
        elif off_meas < 0:
            sig_tx = sig_tx[off:]
            sig_rx = sig_rx[:-off]

        if off % 2 == 1:
            sig_tx = sig_tx[:-1]
            sig_rx = sig_rx[:-1]

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_sync_sample_aligned.png"

            fig, axs = plt.subplots(2)
            axs[0].plot(np.abs(sig_tx[:128]), label="TX Frame")
            axs[0].plot(np.abs(sig_rx[:128]), label="RX Frame")
            axs[0].set_title("Sample Aligned Data")
            axs[0].set_ylabel("Amplitude")
            axs[0].set_xlabel("Samples")
            axs[0].legend(loc=4)

            axs[1].plot(np.real(sig_tx[:128]), label="TX Frame")
            axs[1].plot(np.real(sig_rx[:128]), label="RX Frame")
            axs[1].set_ylabel("Real Part")
            axs[1].set_xlabel("Samples")
            axs[1].legend(loc=4)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

        sig_rx = sa.subsample_align(sig_rx, sig_tx)

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_sync_subsample_aligned.png"

            fig, axs = plt.subplots(2)
            axs[0].plot(np.abs(sig_tx[:128]), label="TX Frame")
            axs[0].plot(np.abs(sig_rx[:128]), label="RX Frame")
            axs[0].set_title("Subsample Aligned")
            axs[0].set_ylabel("Amplitude")
            axs[0].set_xlabel("Samples")
            axs[0].legend(loc=4)

            axs[1].plot(np.real(sig_tx[:128]), label="TX Frame")
            axs[1].plot(np.real(sig_rx[:128]), label="RX Frame")
            axs[1].set_ylabel("Real Part")
            axs[1].set_xlabel("Samples")
            axs[1].legend(loc=4)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

        sig_rx = pa.phase_align(sig_rx, sig_tx)

        if self.plot and self.c.plot_location is not None:
            dt = datetime.datetime.now().isoformat()
            fig_path = self.c.plot_location + "/" + dt + "_sync_phase_aligned.png"

            fig, axs = plt.subplots(2)
            axs[0].plot(np.abs(sig_tx[:128]), label="TX Frame")
            axs[0].plot(np.abs(sig_rx[:128]), label="RX Frame")
            axs[0].set_title("Phase Aligned")
            axs[0].set_ylabel("Amplitude")
            axs[0].set_xlabel("Samples")
            axs[0].legend(loc=4)

            axs[1].plot(np.real(sig_tx[:128]), label="TX Frame")
            axs[1].plot(np.real(sig_rx[:128]), label="RX Frame")
            axs[1].set_ylabel("Real Part")
            axs[1].set_xlabel("Samples")
            axs[1].legend(loc=4)

            fig.tight_layout()
            fig.savefig(fig_path)
            plt.close(fig)

        logging.debug(
            "Sig1_cut: %d %s, Sig2_cut: %d %s, off: %d" % (len(sig_tx), sig_tx.dtype, len(sig_rx), sig_rx.dtype, off))
        return sig_tx, sig_rx

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
