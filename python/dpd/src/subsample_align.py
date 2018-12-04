# -*- coding: utf-8 -*-
#
# DPD Computation Engine, utility to do subsample alignment.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file
import datetime
import logging
import os
import numpy as np
from scipy import optimize
import matplotlib.pyplot as plt

def gen_omega(length):
    if (length % 2) == 1:
        raise ValueError("Needs an even length array.")

    halflength = int(length / 2)
    factor = 2.0 * np.pi / length

    omega = np.zeros(length, dtype=np.float)
    for i in range(halflength):
        omega[i] = factor * i

    for i in range(halflength, length):
        omega[i] = factor * (i - length)

    return omega


def subsample_align(sig, ref_sig, plot_location=None):
    """Do subsample alignment for sig relative to the reference signal
    ref_sig. The delay between the two must be less than sample

    Returns the aligned signal"""

    n = len(sig)
    if (n % 2) == 1:
        raise ValueError("Needs an even length signal.")
    halflen = int(n / 2)

    fft_sig = np.fft.fft(sig)

    omega = gen_omega(n)

    def correlate_for_delay(tau):
        # A subsample offset between two signals corresponds, in the frequency
        # domain, to a linearly increasing phase shift, whose slope
        # corresponds to the delay.
        #
        # Here, we build this phase shift in rotate_vec, and multiply it with
        # our signal.

        rotate_vec = np.exp(1j * tau * omega)
        # zero-frequency is rotate_vec[0], so rotate_vec[N/2] is the
        # bin corresponding to the [-1, 1, -1, 1, ...] time signal, which
        # is both the maximum positive and negative frequency.
        # I don't remember why we handle it differently.
        rotate_vec[halflen] = np.cos(np.pi * tau)

        corr_sig = np.fft.ifft(rotate_vec * fft_sig)

        return -np.abs(np.sum(np.conj(corr_sig) * ref_sig))

    optim_result = optimize.minimize_scalar(correlate_for_delay, bounds=(-1, 1), method='bounded',
                                            options={'disp': True})

    if optim_result.success:
        best_tau = optim_result.x

        if plot_location is not None:
            corr = np.vectorize(correlate_for_delay)
            ixs = np.linspace(-1, 1, 100)
            taus = corr(ixs)

            dt = datetime.datetime.now().isoformat()
            tau_path = (plot_location + "/" + dt + "_tau.png")
            plt.plot(ixs, taus)
            plt.title("Subsample correlation, minimum is best: {}".format(best_tau))
            plt.savefig(tau_path)
            plt.close()

        # Prepare rotate_vec = fft_sig with rotated phase
        rotate_vec = np.exp(1j * best_tau * omega)
        rotate_vec[halflen] = np.cos(np.pi * best_tau)
        return np.fft.ifft(rotate_vec * fft_sig).astype(np.complex64)
    else:
        # print("Could not optimize: " + optim_result.message)
        return np.zeros(0, dtype=np.complex64)

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
