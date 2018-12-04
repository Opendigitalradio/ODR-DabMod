# -*- coding: utf-8 -*-
#
# DPD Computation Engine, Capture TX signal and RX feedback using ODR-DabMod's
# DPD Server.
#
#   Copyright (c) 2017 Andreas Steger
#   Copyright (c) 2018 Matthias P. Braendli
#
#    http://www.opendigitalradio.org
#
#   This file is part of ODR-DabMod.
#
#   ODR-DabMod is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of the
#   License, or (at your option) any later version.
#
#   ODR-DabMod is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

import socket
import struct
import os.path
import logging
import numpy as np
from scipy import signal
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import io

from . import Align as sa

def correlation_coefficient(sig_tx, sig_rx):
    return np.corrcoef(sig_tx, sig_rx)[0, 1]

def align_samples(sig_tx, sig_rx):
    """
    Returns an aligned version of sig_tx and sig_rx by cropping, subsample alignment and
    correct phase offset
    """

    # Coarse sample-level alignment
    c = np.abs(signal.correlate(sig_rx, sig_tx))
    off_meas = np.argmax(c) - sig_tx.shape[0] + 1
    off = int(abs(off_meas))

    if off_meas > 0:
        sig_tx = sig_tx[:-off]
        sig_rx = sig_rx[off:]
    elif off_meas < 0:
        sig_tx = sig_tx[off:]
        sig_rx = sig_rx[:-off]

    if off % 2 == 1:
        sig_tx = sig_tx[:-1]
        sig_rx = sig_rx[:-1]

    # Fine subsample alignment and phase offset
    sig_rx = sa.subsample_align(sig_rx, sig_tx)
    sig_rx = sa.phase_align(sig_rx, sig_tx)
    return sig_tx, sig_rx, abs(off_meas)

class Capture:
    """Capture samples from ODR-DabMod"""
    def __init__(self, samplerate, port, num_samples_to_request, plot_dir):
        self.samplerate = samplerate
        self.sizeof_sample = 8 # complex floats
        self.port = port
        self.num_samples_to_request = num_samples_to_request
        self.plot_dir = plot_dir

        # Before we run the samples through the model, we want to accumulate
        # them into bins depending on their amplitude, and keep only n_per_bin
        # samples to avoid that the polynomial gets overfitted in the low-amplitude
        # part, which is less interesting than the high-amplitude part, where
        # non-linearities become apparent.
        self.binning_n_bins = 64  # Number of bins between binning_start and binning_end
        self.binning_n_per_bin = 128  # Number of measurements pre bin

        self.rx_normalisation = 1.0

        self.clear_accumulated()

    def clear_accumulated(self):
        self.binning_start = 0.0
        self.binning_end = 1.0

        # axis 0: bins
        # axis 1: 0=tx, 1=rx
        self.accumulated_bins = [np.zeros((0, 2), dtype=np.complex64) for i in range(self.binning_n_bins)]

    def _recv_exact(self, sock, num_bytes):
        """Receive an exact number of bytes from a socket. This is
        a wrapper around sock.recv() that can return less than the number
        of requested bytes.

        Args:
            sock (socket): Socket to receive data from.
            num_bytes (int): Number of bytes that will be returned.
        """
        bufs = []
        while num_bytes > 0:
            b = sock.recv(num_bytes)
            if len(b) == 0:
                break
            num_bytes -= len(b)
            bufs.append(b)
        return b''.join(bufs)

    def receive_tcp(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(4)
        s.connect(('localhost', self.port))

        logging.debug("Send version")
        s.sendall(b"\x01")

        logging.debug("Send request for {} samples".format(self.num_samples_to_request))
        s.sendall(struct.pack("=I", self.num_samples_to_request))

        logging.debug("Wait for TX metadata")
        num_samps, tx_second, tx_pps = struct.unpack("=III", self._recv_exact(s, 12))
        tx_ts = tx_second + tx_pps / 16384000.0

        if num_samps > 0:
            logging.debug("Receiving {} TX samples".format(num_samps))
            txframe_bytes = self._recv_exact(s, num_samps * self.sizeof_sample)
            txframe = np.fromstring(txframe_bytes, dtype=np.complex64)
        else:
            txframe = np.array([], dtype=np.complex64)

        logging.debug("Wait for RX metadata")
        rx_second, rx_pps = struct.unpack("=II", self._recv_exact(s, 8))
        rx_ts = rx_second + rx_pps / 16384000.0

        if num_samps > 0:
            logging.debug("Receiving {} RX samples".format(num_samps))
            rxframe_bytes = self._recv_exact(s, num_samps * self.sizeof_sample)
            rxframe = np.fromstring(rxframe_bytes, dtype=np.complex64)
        else:
            rxframe = np.array([], dtype=np.complex64)

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            logging.debug('txframe: min {}, max {}, median {}'.format(
                          np.min(np.abs(txframe)),
                          np.max(np.abs(txframe)),
                          np.median(np.abs(txframe))))

            logging.debug('rxframe: min {}, max {}, median {}'.format(
                          np.min(np.abs(rxframe)),
                          np.max(np.abs(rxframe)),
                          np.median(np.abs(rxframe))))

        logging.debug("Disconnecting")
        s.close()

        return txframe, tx_ts, rxframe, rx_ts

    def _plot_spectrum(self, signal, filename, title):
        fig = plt.figure()
        ax = plt.subplot(1, 1, 1)

        fft = np.fft.fftshift(np.fft.fft(signal))
        fft_db = 20 * np.log10(np.abs(fft))

        ax.plot(fft_db)
        ax.set_title(title)
        fig.tight_layout()
        fig.savefig(os.path.join(self.plot_dir, filename))
        plt.close(fig)

    def calibrate(self):
        txframe, tx_ts, rxframe, rx_ts = self.receive_tcp()

        # Normalize received signal with sent signal
        tx_median = np.median(np.abs(txframe))
        rx_median = np.median(np.abs(rxframe))
        self.rx_normalisation = tx_median / rx_median

        rxframe = rxframe * self.rx_normalisation
        txframe_aligned, rxframe_aligned, coarse_offset = align_samples(txframe, rxframe)

        self._plot_spectrum(rxframe[:8192], "rxframe.png", "RX Frame")
        self._plot_spectrum(txframe[:8192], "txframe.png", "RX Frame")

        return tx_ts, tx_median, rx_ts, rx_median, np.abs(coarse_offset), correlation_coefficient(txframe_aligned, rxframe_aligned)

    def get_samples(self):
        """Connect to ODR-DabMod, retrieve TX and RX samples, load
        into numpy arrays, and return a tuple
        (txframe_aligned, tx_ts, tx_median, rxframe_aligned, rx_ts, rx_median)
        """

        txframe, tx_ts, rxframe, rx_ts = self.receive_tcp()

        # Normalize received signal with calibrated normalisation
        rxframe = rxframe * self.rx_normalisation
        txframe_aligned, rxframe_aligned, coarse_offset = align_samples(txframe, rxframe)
        self._bin_and_accumulate(txframe_aligned, rxframe_aligned)
        return txframe_aligned, tx_ts, tx_median, rxframe_aligned, rx_ts, rx_median

    def bin_histogram(self):
        return [b.shape[0] for b in self.accumulated_bins]

    def pointcloud_png(self):
        fig = plt.figure()
        ax = plt.subplot(1, 1, 1)
        for b in self.accumulated_bins:
            if b:
                ax.scatter(
                        np.abs(b[0]),
                        np.abs(b[1]),
                        s=0.1,
                        color="black")
        ax.set_title("Captured and Binned Samples")
        ax.set_xlabel("TX Amplitude")
        ax.set_ylabel("RX Amplitude")
        ax.set_ylim(0, 0.8)
        ax.set_xlim(0, 1.1)
        ax.legend(loc=4)
        fig.tight_layout()
        fig.savefig(os.path.join(self.plot_dir, "pointcloud.png"))
        plt.close(fig)

    def _bin_and_accumulate(self, txframe, rxframe):
        """Bin the samples and extend the accumulated samples"""

        bin_edges = np.linspace(self.binning_start, self.binning_end, self.binning_n_bins)

        minsize = self.num_samples_to_request

        for i, (tx_start, tx_end) in enumerate(zip(bin_edges, bin_edges[1:])):
            txframe_abs = np.abs(txframe)
            indices = np.bitwise_and(tx_start < txframe_abs, txframe_abs <= tx_end)
            txsamples = np.asmatrix(txframe[indices])
            rxsamples = np.asmatrix(rxframe[indices])
            binned_sample_pairs = np.concatenate((txsamples, rxsamples)).T

            missing_in_bin = self.binning_n_per_bin - self.accumulated_bins[i].shape[0]
            num_to_append = min(missing_in_bin, binned_sample_pairs.shape[0])
            print("Handling bin {} {}-{}, {} available, {} missing".format(i, tx_start, tx_end, binned_sample_pairs.shape[0], missing_in_bin))
            if num_to_append:
                print("Appending {} to bin {} with shape {}".format(num_to_append, i, self.accumulated_bins[i].shape))

                self.accumulated_bins[i] = np.concatenate((self.accumulated_bins[i], binned_sample_pairs[:num_to_append,...]))
                print("{} now has shape {}".format(i, self.accumulated_bins[i].shape))

