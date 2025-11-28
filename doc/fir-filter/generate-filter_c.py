#!/usr/bin/env python
# This tool uses gnuradio to generate FIR filter taps and then zero-pads them
# for a target length of 127.
#
# Usage: python zero_pad_taps.py > fir_taps_127.h

import gnuradio
from gnuradio import digital
import numpy as np

# --- Filter Design Settings (Original) ---
gain = 1
sampling_freq = 2.048e6 * 4
cutoff = 810e3
transition_width = 250e3

# --- Target Length Setting (New) ---
TARGET_LEN = 127

# --- Generate filter taps ---
# Hamming window is used by default
# This will generate approximately 79 taps based on the design parameters
taps_original = digital.filter.firdes.low_pass(gain, sampling_freq, cutoff, transition_width)

# Ensure taps_original is a standard list for easy manipulation
taps_original = list(taps_original)
num_taps_original = len(taps_original)

# --- Zero-Padding Logic ---
if num_taps_original > TARGET_LEN:
    raise ValueError("Original filter length exceeds target length. Cannot pad.")

total_zeros = TARGET_LEN - num_taps_original
# Calculate leading and trailing zeros, ensuring the padding is centered
# and handling cases where total_zeros is odd (though 48 is even here)
leading_zeros = total_zeros // 2
trailing_zeros = total_zeros - leading_zeros

# Create the zero-padded array
taps_padded = ([0.0] * leading_zeros) + taps_original + ([0.0] * trailing_zeros)
num_taps_padded = len(taps_padded)

# --- C++ Style Output Generation ---
print(f"// Generated FIR Filter Taps for ODR-DabMod")
print(f"// Original Design: {num_taps_original} Taps, Zero-Padded to {num_taps_padded} Taps")
print(f"// Padding: {leading_zeros} leading zeros, {trailing_zeros} trailing zeros")
print(f"// Designed with gnuradio firdes.low_pass (Hamming Window)")
print(f"// Parameters:")
print(f"//   Sampling Frequency (fs): {sampling_freq / 1e6:.3f} MHz")
print(f"//   Cutoff Frequency (fc): {cutoff / 1e3:.1f} kHz")
print(f"//   Transition Width (df): {transition_width / 1e3:.1f} kHz")
print(f"//   Filter Gain: {gain:.1f}\n")

print(f"const int FIR_TAPS_COUNT = {num_taps_padded}; // Must be 127")
print(f"const double fir_taps[FIR_TAPS_COUNT] = {{")

# Format and print the padded taps
for i, t in enumerate(taps_padded):
    # Use numpy for consistent scientific notation formatting
    tap_str = np.format_float_scientific(t, precision=8, unique=False)
    
    suffix = ",\n"
    if i == num_taps_padded - 1:
        suffix = "\n"
        
    print(f"    {tap_str}{suffix}", end="")

print(f"}};\n")