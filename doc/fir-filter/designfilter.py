#!/usr/bin/env python
# This tool uses pm_remez to generate FIR filter taps and outputs them
# as a C-style header.
#
# Usage: python generate-filter_c.py > fir_taps.h

import pm_remez
import numpy as np
import scipy.signal

# --- Filter Design Settings ---
decimation = 4
transition_bandwidth = 0.1
numtaps = 127
stopband_weight = 1.0
one_over_f = False
gain = 21

# --- Design the filter using pm_remez (from designcoe.py) ---
passband_end = 0.5 * (1 - transition_bandwidth) / decimation
stopband_start = 0.5 * (1 + transition_bandwidth) / decimation

sweight = ((stopband_weight, stopband_weight * 0.5 / stopband_start)
           if one_over_f else stopband_weight)

design = pm_remez.remez(
    numtaps, [0, passband_end, stopband_start, 0.5],
    [1, 0], weight=[1, sweight])

# Normalize so that the maximum coefficient is 1.0
taps = np.array(design.impulse_response)

dc_gain = np.sum(taps)
taps = taps * (decimation / dc_gain)

num_taps = len(taps)

# --- C++ Style Output Generation ---
print(f"// Generated FIR Filter Taps for ODR-DabMod")
print(f"// Designed with pm_remez (Parks-McClellan)")
print(f"// Parameters:")
print(f"//   Number of taps: {num_taps}")
print(f"//   Decimation: {decimation}")
print(f"//   Transition bandwidth: {transition_bandwidth}")
print(f"//   Passband end: {passband_end:.6f}")
print(f"//   Stopband start: {stopband_start:.6f}")
print(f"//   Stopband attenuation: {20 * np.log10(design.weighted_error / stopband_weight):.2f} dB")
print(f"//   Passband ripple: {20 * np.log10(1 + design.weighted_error):.4f} dB\n")

print(f"const int FIR_TAPS_COUNT = {num_taps};")
print(f"const double fir_taps[FIR_TAPS_COUNT] = {{")

# Format and print the taps
for i, t in enumerate(taps):
    tap_str = np.format_float_scientific(t, precision=8, unique=False)

    suffix = ",\n"
    if i == num_taps - 1:
        suffix = "\n"

    print(f"    {tap_str}{suffix}", end="")

print(f"}};")