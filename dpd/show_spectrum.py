#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This is an example tool that shows how to connect to ODR-DabMod's dpd TCP server
# and get samples from there.
#
# Since the TX and RX samples are not perfectly aligned, the tool has to align them properly,
# which is done in two steps: First on sample-level using a correlation, then with subsample
# accuracy using a FFT approach.
#
# It requires SciPy and matplotlib.
#
# Copyright (C) 2017 Matthias P. Braendli
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import sys
import socket
import struct
import numpy as np
import matplotlib.pyplot as pp
import scipy.signal

SIZEOF_SAMPLE = 8 # complex floats

if len(sys.argv) != 3:
    print("Usage: show_spectrum.py <port> <number of samples to request>")
    sys.exit(1)

port = int(sys.argv[1])
num_samps_to_request = int(sys.argv[2])

def recv_exact(sock, num_bytes):
    bufs = []
    while num_bytes > 0:
        b = sock.recv(num_bytes)
        if len(b) == 0:
            break
        num_bytes -= len(b)
        bufs.append(b)
    return b''.join(bufs)

def get_samples(port, num_samps_to_request):
    """Connect to ODR-DabMod, retrieve TX and RX samples, load
    into numpy arrays, and return a tuple
    (tx_timestamp, tx_samples, rx_timestamp, rx_samples)
    where the timestamps are doubles, and the samples are numpy
    arrays of complex floats, both having the same size
    """

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', port))

    print("Send version");
    s.sendall(b"\x01")

    print("Send request for {} samples".format(num_samps_to_request))
    s.sendall(struct.pack("=I", num_samps_to_request))

    print("Wait for TX metadata")
    num_samps, tx_second, tx_pps = struct.unpack("=III", recv_exact(s, 12))
    tx_ts = tx_second + tx_pps / 16384000.0

    if num_samps > 0:
        print("Receiving {} TX samples".format(num_samps))
        txframe_bytes = recv_exact(s, num_samps * SIZEOF_SAMPLE)
        txframe = np.fromstring(txframe_bytes, dtype=np.complex64)
    else:
        txframe = np.array([], dtype=np.complex64)

    print("Wait for RX metadata")
    rx_second, rx_pps = struct.unpack("=II", recv_exact(s, 8))
    rx_ts = rx_second + rx_pps / 16384000.0

    if num_samps > 0:
        print("Receiving {} RX samples".format(num_samps))
        rxframe_bytes = recv_exact(s, num_samps * SIZEOF_SAMPLE)
        rxframe = np.fromstring(rxframe_bytes, dtype=np.complex64)
    else:
        rxframe = np.array([], dtype=np.complex64)

    print("Disconnecting")
    s.close()

    return (tx_ts, txframe, rx_ts, rxframe)


tx_ts, txframe, rx_ts, rxframe = get_samples(port, num_samps_to_request)

print("Received {} & {} frames at {} and {}".format(
    len(txframe), len(rxframe), tx_ts, rx_ts))

print("Calculate TX and RX spectrum assuming 8192000 samples per second")
fft_size = 4096
tx_spectrum = np.fft.fftshift(np.fft.fft(txframe, fft_size))
tx_power = 20*np.log10(np.abs(tx_spectrum))

rx_spectrum = np.fft.fftshift(np.fft.fft(rxframe, fft_size))
rx_power = 20*np.log10(np.abs(rx_spectrum))

sampling_rate = 8192000
freqs = np.fft.fftshift(np.fft.fftfreq(fft_size, d=1./sampling_rate))

fig = pp.figure()

fig.suptitle("TX and RX spectrum")
ax1 = fig.add_subplot(211)
ax1.set_title("TX")
ax1.plot(freqs, tx_power)
ax2 = fig.add_subplot(212)
ax2.set_title("RX")
ax2.plot(freqs, rx_power)
pp.show()

# The MIT License (MIT)
#
# Copyright (c) 2017 Matthias P. Braendli
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
