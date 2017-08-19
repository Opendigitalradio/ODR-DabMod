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

    dt = datetime.datetime.now().isoformat()
    fig_path = "/tmp/phase_align_" + dt + ".pdf"
    plt.subplot(311)
    plt.hist(angle_diff, bins=60, label="Angle Diff")
    plt.xlabel("Angle")
    plt.ylabel("Count")
    plt.legend(loc=4)

    plt.subplot(312)
    plt.plot(np.angle(ref_sig[:128]), label="ref_sig")
    plt.plot(np.angle(sig[:128]), label="sig")
    plt.xlabel("Angle")
    plt.ylabel("Sample")
    plt.legend(loc=4)

    angle = np.median(angle_diff)
    logging.debug("Compensating phase by {} rad, {} degree.".format(
        angle, angle*180./np.pi
    ))
    sig = sig * np.exp(1j * -angle)

    plt.subplot(313)
    plt.plot(np.angle(ref_sig[:128]), label="ref_sig")
    plt.plot(np.angle(sig[:128]), label="sig")
    plt.xlabel("Angle")
    plt.ylabel("Sample")
    plt.legend(loc=4)
    plt.tight_layout()
    plt.savefig(fig_path)
    plt.clf()

    return sig
