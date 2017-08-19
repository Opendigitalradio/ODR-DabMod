# -*- coding: utf-8 -*-

import sys
import socket
import struct
import numpy as np
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import argparse
import os
import time
import logging
import src.Dab_Util as DU
import datetime

class Measure:
    """Collect Measurement from DabMod"""
    def __init__(self, samplerate, port, num_samples_to_request):
        logging.info("Instantiate Measure object")
        self.samplerate = samplerate
        self.sizeof_sample = 8 # complex floats
        self.port = port
        self.num_samples_to_request = num_samples_to_request

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

    def get_samples(self):
        """Connect to ODR-DabMod, retrieve TX and RX samples, load
        into numpy arrays, and return a tuple
        (tx_timestamp, tx_samples, rx_timestamp, rx_samples)
        where the timestamps are doubles, and the samples are numpy
        arrays of complex floats, both having the same size
        """
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
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

        txframe = txframe / np.median(np.abs(txframe))
        rxframe = rxframe / np.median(np.abs(rxframe))

        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            txframe_path = ('/tmp/txframe_fft_' +
                            datetime.datetime.now().isoformat() +
                            '.pdf')
            plt.plot(np.abs(np.fft.fftshift(np.fft.fft(txframe[:self.samplerate]))))
            plt.savefig(txframe_path)
            plt.clf()

            rxframe_path = ('/tmp/rxframe_fft_' +
                            datetime.datetime.now().isoformat() +
                            '.pdf')
            plt.plot(np.abs(np.fft.fftshift(np.fft.fft(rxframe[:self.samplerate]))))
            plt.savefig(rxframe_path)
            plt.clf()

            logging.debug("txframe: min %f, max %f, median %f, spectrum %s" %
                (np.min(np.abs(txframe)),
                 np.max(np.abs(txframe)),
                 np.median(np.abs(txframe)),
                 txframe_path))

            logging.debug("rxframe: min %f, max %f, median %f, spectrum %s" %
                (np.min(np.abs(rxframe)),
                 np.max(np.abs(rxframe)),
                 np.median(np.abs(rxframe)),
                 rxframe_path))

        logging.debug("Disconnecting")
        s.close()

        du = DU.Dab_Util(self.samplerate)
        txframe_aligned, rxframe_aligned = du.subsample_align(txframe, rxframe)

        logging.info(
            "Measurement done, tx %d %s, rx %d %s, tx aligned %d %s, rx aligned %d %s"
            % (len(txframe), txframe.dtype, len(rxframe), rxframe.dtype,
            len(txframe_aligned), txframe_aligned.dtype, len(rxframe_aligned), rxframe_aligned.dtype) )

        return txframe_aligned, tx_ts, rxframe_aligned, rx_ts

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
