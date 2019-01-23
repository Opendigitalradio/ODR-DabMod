# -*- coding: utf-8 -*-
#
# DPD Computation Engine, Measure signal using ODR-DabMod's
# DPD Server.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import socket
import struct
import numpy as np
import dpd.Dab_Util as DU
import os
import logging

class Measure:
    """Collect Measurement from DabMod"""
    def __init__(self, config, samplerate : int, port : int, num_samples_to_request : int):
        logging.info("Instantiate Measure object")
        self.c = config
        self.samplerate = samplerate
        self.sizeof_sample = 8 # complex floats
        self.port = port
        self.num_samples_to_request = num_samples_to_request

    def _recv_exact(self, sock : socket.socket, num_bytes : int) -> bytes:
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

    def receive_tcp(self, num_samples_to_request : int):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(4)
        s.connect(('localhost', self.port))

        logging.debug("Send version")
        s.sendall(b"\x01")

        logging.debug("Send request for {} samples".format(num_samples_to_request))
        s.sendall(struct.pack("=I", num_samples_to_request))

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

    def get_samples_unaligned(self, short=False):
        """Connect to ODR-DabMod, retrieve TX and RX samples, load
        into numpy arrays, and return a tuple
        (txframe, tx_ts, rxframe, rx_ts, rx_median, tx_median)
        """

        n_samps = int(self.num_samples_to_request / 4) if short else self.num_samples_to_request
        txframe, tx_ts, rxframe, rx_ts = self.receive_tcp(n_samps)

        # Normalize received signal with sent signal
        rx_median = np.median(np.abs(rxframe))
        tx_median = np.median(np.abs(txframe))
        rxframe = rxframe / rx_median * tx_median


        logging.info(
            "Measurement done, tx %d %s, rx %d %s" %
            (len(txframe), txframe.dtype, len(rxframe), rxframe.dtype))

        return txframe, tx_ts, rxframe, rx_ts, rx_median, tx_median

    def get_samples(self, short=False):
        """Connect to ODR-DabMod, retrieve TX and RX samples, load
        into numpy arrays, and return a tuple
        (txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median, tx_median)
        """

        n_samps = int(self.num_samples_to_request / 4) if short else self.num_samples_to_request
        txframe, tx_ts, rxframe, rx_ts = self.receive_tcp(n_samps)

        # Normalize received signal with sent signal
        rx_median = np.median(np.abs(rxframe))
        tx_median = np.median(np.abs(txframe))
        rxframe = rxframe / rx_median * tx_median

        du = DU.Dab_Util(self.c, self.samplerate)
        txframe_aligned, rxframe_aligned = du.subsample_align(txframe, rxframe)

        logging.info(
            "Measurement done, tx %d %s, rx %d %s, tx aligned %d %s, rx aligned %d %s"
            % (len(txframe), txframe.dtype, len(rxframe), rxframe.dtype,
            len(txframe_aligned), txframe_aligned.dtype, len(rxframe_aligned), rxframe_aligned.dtype) )

        return txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median, tx_median

# The MIT License (MIT)
#
# Copyright (c) 2018 Matthias P. Braendli
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
