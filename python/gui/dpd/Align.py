# -*- coding: utf-8 -*-
#
# DPD Computation Engine, utility to do subsample alignment.
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
import datetime
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

def phase_align(sig, ref_sig, plot_location=None):
    """Do phase alignment for sig relative to the reference signal
    ref_sig.

    Returns the aligned signal"""

    angle_diff = (np.angle(sig) - np.angle(ref_sig)) % (2. * np.pi)

    real_diffs = np.cos(angle_diff)
    imag_diffs = np.sin(angle_diff)

    if plot_location is not None:
        dt = datetime.datetime.now().isoformat()
        fig_path = plot_location + "/" + dt + "_phase_align.png"

        plt.subplot(511)
        plt.hist(angle_diff, bins=60, label="Angle Diff")
        plt.xlabel("Angle")
        plt.ylabel("Count")
        plt.legend(loc=4)

        plt.subplot(512)
        plt.hist(real_diffs, bins=60, label="Real Diff")
        plt.xlabel("Real Part")
        plt.ylabel("Count")
        plt.legend(loc=4)

        plt.subplot(513)
        plt.hist(imag_diffs, bins=60, label="Imaginary Diff")
        plt.xlabel("Imaginary Part")
        plt.ylabel("Count")
        plt.legend(loc=4)

        plt.subplot(514)
        plt.plot(np.angle(ref_sig[:128]), label="ref_sig")
        plt.plot(np.angle(sig[:128]), label="sig")
        plt.xlabel("Angle")
        plt.ylabel("Sample")
        plt.legend(loc=4)

    real_diff = np.median(real_diffs)
    imag_diff = np.median(imag_diffs)

    angle = np.angle(real_diff + 1j * imag_diff)

    #logging.debug( "Compensating phase by {} rad, {} degree. real median {}, imag median {}".format( angle, angle*180./np.pi, real_diff, imag_diff))
    sig = sig * np.exp(1j * -angle)

    if plot_location is not None:
        plt.subplot(515)
        plt.plot(np.angle(ref_sig[:128]), label="ref_sig")
        plt.plot(np.angle(sig[:128]), label="sig")
        plt.xlabel("Angle")
        plt.ylabel("Sample")
        plt.legend(loc=4)
        plt.tight_layout()
        plt.savefig(fig_path)
        plt.close()

    return sig
