#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This is an example tool that shows how to connect to ODR-DabMod's dpd TCP
# server and get samples from there.
#
# Since the TX and RX samples are not perfectly aligned, the tool has to align
# them properly, which is done in two steps: First on sample-level using a
# correlation, then with subsample accuracy using a FFT approach.
#
# It requires SciPy and matplotlib.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import sys
import socket
import struct
import numpy as np
import matplotlib.pyplot as pp
from matplotlib.animation import FuncAnimation
import argparse
from scipy.misc import imsave

SIZEOF_SAMPLE = 8 # complex floats

# Constants for TM 1
NbSymbols = 76
NbCarriers = 1536
Spacing = 2048
NullSize = 2656
SymSize = 2552
FicSizeOut = 288

def main():
    parser = argparse.ArgumentParser(description="Plot the spectrum of ODR-DabMod's DPD feedback")
    parser.add_argument('--samps', default='10240', help='Number of samples to request at once',
            required=False)
    parser.add_argument('--port', default='50055',
            help='port to connect to ODR-DabMod DPD (default: 50055)',
            required=False)
    parser.add_argument('--animated', action='store_true', help='Enable real-time animation')
    parser.add_argument('--constellation', action='store_true', help='Draw constellaton plot')
    parser.add_argument('--samplerate', default='8192000', help='Sample rate',
            required=False)

    cli_args = parser.parse_args()

    if cli_args.constellation:
        plot_constellation_once(cli_args)
    elif cli_args.animated:
        plot_spectrum_animated(cli_args)
    else:
        plot_spectrum_once(cli_args)

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

def recv_rxtx(port, num_samps_to_request):
    tx_ts, txframe, rx_ts, rxframe = get_samples(port, num_samps_to_request)

    # convert to complex doubles for more dynamic range
    txframe = txframe.astype(np.complex128)
    rxframe = rxframe.astype(np.complex128)

    print("Received {} & {} frames at {} and {}".format(
        len(txframe), len(rxframe), tx_ts, rx_ts))
    return tx_ts, txframe, rx_ts, rxframe

def get_spectrum(port, num_samps_to_request):
    tx_ts, txframe, rx_ts, rxframe = recv_rxtx(port, num_samps_to_request)

    print("Calculate TX and RX spectrum assuming 8192000 samples per second")
    tx_spectrum = np.fft.fftshift(np.fft.fft(txframe, fft_size))
    tx_power = 20*np.log10(np.abs(tx_spectrum))

    rx_spectrum = np.fft.fftshift(np.fft.fft(rxframe, fft_size))
    rx_power = 20*np.log10(np.abs(rx_spectrum))
    return tx_power, rx_power

def remove_guard_intervals(frame, options):
    """Remove the cyclic prefix. The frame needs to be aligned to the
    end of the transmission frame. Transmission Mode 1 is assumed"""
    oversample = int(int(options.samplerate) / 2048000)

    # From the end, take 2048 samples, then skip 504 samples
    frame = frame[::-1]

    stride_len = Spacing * oversample
    stride_advance = SymSize * oversample

    # Truncate the frame to an integer length of strides
    newlen = len(frame) - (len(frame) % stride_advance)
    print("Truncating frame from {} to {}".format(len(frame), newlen))
    frame = frame[:newlen]

    # Remove the cyclic prefix
    frame = frame.reshape(-1, stride_advance)[:,:stride_len].reshape(-1)

    # Reverse again
    return frame[::-1]


def plot_constellation_once(options):
    port = int(options.port)
    num_samps_to_request = int(options.samps)

    tx_ts, txframe, rx_ts, rxframe = recv_rxtx(port, num_samps_to_request)

    frame = remove_guard_intervals(txframe, options)

    oversample = int(int(options.samplerate) / 2048000)

    n = Spacing * oversample # is also number of samples per symbol
    if len(frame) % n != 0:
        raise ValueError("Frame length doesn't contain exact number of symbols")
    num_syms = int(len(frame) / n)
    print("frame {} has {} symbols".format(len(frame), num_syms))
    spectrums = np.array([np.fft.fftshift(np.fft.fft(frame[n*i:n*(i+1)], n)) for i in range(num_syms)])

    def normalise(x):
        """Normalise a real-valued array x to the range [0,1]"""
        y = x + np.min(x)
        return x / np.max(x)

    imsave("spectrums.png", np.concatenate([
        normalise(np.abs(spectrums)),
        normalise(np.angle(spectrums))]))

    # Only take bins that are supposed to contain energy
    # i.e. the middle 1536 bins, excluding the bin at n/2
    assert(n % 2 == 0)
    n_half = int(n/2)
    spectrums = np.concatenate(
            [spectrums[...,n_half-768:n_half],
             spectrums[...,n_half + 1:n_half + 769]], axis=1)

    sym_indices = (np.tile(np.arange(num_syms-1).reshape(num_syms-1,1), (1,NbCarriers)) +
                   np.tile(np.linspace(-0.4, 0.4, NbCarriers), (num_syms-1, 1) ) )
    sym_indices = sym_indices.reshape(-1)
    diff_angles = np.mod(np.diff(np.angle(spectrums, deg=1), axis=0), 360)
    #sym_points = spectrums[:-1].reshape(-1)
    # Set amplitude and phase of low power points to zero, avoid cluttering diagram
    #sym_points[np.abs(sym_points) < np.mean(np.abs(sym_points)) * 0.1] = 0

    print("ix {}  spec {} da {}".format(
        sym_indices.shape, spectrums.shape, diff_angles.shape))

    fig = pp.figure()

    fig.suptitle("Constellation")
    ax1 = fig.add_subplot(111)
    ax1.set_title("TX")
    ax1.scatter(sym_indices, diff_angles.reshape(-1), alpha=0.1)

    pp.show()

fft_size = 4096

def plot_spectrum_once(options):
    port = int(options.port)
    num_samps_to_request = int(options.samps)
    freqs = np.fft.fftshift(np.fft.fftfreq(fft_size, d=1./int(options.samplerate)))

    tx_power, rx_power = get_spectrum(port, num_samps_to_request)
    fig = pp.figure()

    fig.suptitle("TX and RX spectrum")
    ax1 = fig.add_subplot(211)
    ax1.set_title("TX")
    ax1.plot(freqs, tx_power, 'r')
    ax2 = fig.add_subplot(212)
    ax2.set_title("RX")
    ax2.plot(freqs, rx_power, 'b')
    pp.show()

def plot_spectrum_animated(options):
    port = int(options.port)
    num_samps_to_request = int(options.samps)
    freqs = np.fft.fftshift(np.fft.fftfreq(fft_size, d=1./int(options.samplerate)))

    fig, axes = pp.subplots(2, sharex=True)
    line1, = axes[0].plot(freqs, np.ones(len(freqs)), 'r', animated=True)
    axes[0].set_title("TX")
    line2, = axes[1].plot(freqs, np.ones(len(freqs)), 'b', animated=True)
    axes[1].set_title("RX")
    lines = [line1, line2]

    axes[0].set_ylim(-30, 50)
    axes[1].set_ylim(-60, 40)

    def update(frame):
        tx_power, rx_power = get_spectrum(port, num_samps_to_request)

        lines[0].set_ydata(tx_power)
        lines[1].set_ydata(rx_power)
        return lines

    ani = FuncAnimation(fig, update, blit=True)
    pp.show()

main()

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
