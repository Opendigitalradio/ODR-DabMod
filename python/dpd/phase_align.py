# -*- coding: utf-8 -*-
#
# DPD Computation Engine, phase-align a signal against a reference.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file
import datetime
import os
import logging
import numpy as np
import matplotlib.pyplot as plt


def phase_align(sig, ref_sig, plot=False):
    """Do phase alignment for sig relative to the reference signal
    ref_sig.

    Returns the aligned signal"""

    angle_diff = (np.angle(sig) - np.angle(ref_sig)) % (2. * np.pi)

    real_diffs = np.cos(angle_diff)
    imag_diffs = np.sin(angle_diff)

    if plot and self.c.plot_location is not None:
        dt = datetime.datetime.now().isoformat()
        fig_path = self.c.plot_location + "/" + dt + "_phase_align.png"

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

    logging.debug(
        "Compensating phase by {} rad, {} degree. real median {}, imag median {}".format(
        angle, angle*180./np.pi, real_diff, imag_diff
    ))
    sig = sig * np.exp(1j * -angle)

    if logging.getLogger().getEffectiveLevel() == logging.DEBUG and plot:
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
