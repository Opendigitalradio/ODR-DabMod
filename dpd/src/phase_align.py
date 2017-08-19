import numpy as np
from scipy import signal, optimize
import sys
import matplotlib.pyplot as plt
import datetime
import logging


def phase_align(sig, ref_sig):
    """Do phase alignment for sig relative to the reference signal
    ref_sig.

    Returns the aligned signal"""

    angle_diff = (np.angle(sig) - np.angle(ref_sig)) % (2. * np.pi)

    real_diffs = np.cos(angle_diff)
    imag_diffs = np.sin(angle_diff)

    if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
        dt = datetime.datetime.now().isoformat()
        fig_path = "/tmp/" + dt + "_phase_align.pdf"

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

    if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
        plt.subplot(515)
        plt.plot(np.angle(ref_sig[:128]), label="ref_sig")
        plt.plot(np.angle(sig[:128]), label="sig")
        plt.xlabel("Angle")
        plt.ylabel("Sample")
        plt.legend(loc=4)
        plt.tight_layout()
        plt.savefig(fig_path)
        plt.clf()

    return sig
