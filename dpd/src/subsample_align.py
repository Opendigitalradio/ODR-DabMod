import numpy as np
from scipy import signal, optimize
import sys
import matplotlib.pyplot as plt
import datetime

def gen_omega(length):
    if (length % 2) == 1:
        raise ValueError("Needs an even length array.")

    halflength = int(length/2)
    factor = 2.0 * np.pi / length

    omega = np.zeros(length, dtype=np.float)
    for i in range(halflength):
        omega[i] = factor * i

    for i in range(halflength, length):
        omega[i] = factor * (i - length)

    return omega

def subsample_align(sig, ref_sig):
    """Do subsample alignment for sig relative to the reference signal
    ref_sig. The delay between the two must be less than sample

    Returns the aligned signal"""

    n = len(sig)
    if (n % 2) == 1:
        raise ValueError("Needs an even length signal.")
    halflen = int(n/2)

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

    optim_result = optimize.minimize_scalar(correlate_for_delay, bounds=(-1,1), method='bounded', options={'disp': True})

    if optim_result.success:
        best_tau = optim_result.x

        #print("Found subsample delay = {}".format(best_tau))

        if 1:
            corr = np.vectorize(correlate_for_delay)
            ixs = np.linspace(-1, 1, 100)
            taus = corr(ixs)

            dt = datetime.datetime.now().isoformat()
            tau_path = ("/tmp/" + dt + "_tau.pdf")
            plt.plot(ixs, taus)
            plt.title("Subsample correlation, minimum is best: {}".format(best_tau))
            plt.savefig(tau_path)
            plt.clf()

        # Prepare rotate_vec = fft_sig with rotated phase
        rotate_vec = np.exp(1j * best_tau * omega)
        rotate_vec[halflen] = np.cos(np.pi * best_tau)
        return np.fft.ifft(rotate_vec * fft_sig).astype(np.complex64)
    else:
        #print("Could not optimize: " + optim_result.message)
        return np.zeros(0, dtype=np.complex64)
