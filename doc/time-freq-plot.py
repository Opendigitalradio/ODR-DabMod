#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Print scope and spectrum from ODR-DabMod I/Q file
#
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
#    The above copyright notice and this permission notice shall be included in
#    all copies or substantial portions of the Software.
#
#    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#    THE SOFTWARE.
import sys
import matplotlib.pyplot as plt
import numpy as np

rate=2048000
# T = 1/2048000 s
# NULL symbol is 2656 T (about 1.3ms) long.
T_NULL = 2656
# Full transmission frame in TM1 is 96ms = 196608 T.
T_TF = 196608

num_skip_samples = 8 * T_TF
num_analyse_samples = 2 * T_TF

if len(sys.argv) < 2:
    print("Specify .iq file name")
    print("Expected format: complex float I/Q, 2048000 Sps")
    print("The input file must contain at least 10 transmission frames,")
    print("i.e. {} samples = {} seconds".format(T_TF * 10, T_TF * 10.0 / rate))
    sys.exit(1)

fd = open(sys.argv[1], 'rb')

# The IQ files potentially have zero samples in the beginning, we need
# to skip a few transmission frames


source_data = np.fromfile(file=fd, dtype=np.complex64, count=num_skip_samples + num_analyse_samples)

print("Read in {} samples".format(len(source_data)))

source_data = source_data[num_skip_samples:]
source_data_time = np.linspace(0, num_analyse_samples/rate, len(source_data))

print("Signal power: {} of {} samples".format(np.sum(np.abs(source_data**2)), len(source_data)))

fft_size = 4096

plt.figure(figsize=(10,8))
plt.subplot(211)
plt.title("Real part of signal")
plt.plot(source_data_time, np.real(source_data))

signal_spectrum = np.abs(np.fft.fftshift(np.fft.fft(source_data[T_NULL:], fft_size)))
freqs = np.fft.fftshift(np.fft.fftfreq(fft_size, d=1./rate))

plt.subplot(212)
plt.title("Spectrum of {} samples after the NULL symbol".format(fft_size))
plt.semilogy(freqs, signal_spectrum)

plt.show()
