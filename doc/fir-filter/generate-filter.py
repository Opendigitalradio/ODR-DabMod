#!/usr/bin/env python
# This tool uses gnuradio to generate FIR filter taps
# that can be used for the FIRFilter function in
# ODR-DabMod
#
# Usage:
#  1) adapt the filter settings below
#  2) Call this script and redirect the output of this script into a file
#
# Requires:
#  A recent gnuradio version (3.7)
#
#
# The MIT License (MIT)
#
# Copyright (c) 2013 Matthias P. Braendli
# http://mpb.li
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

import gnuradio
from gnuradio import digital

# From documentation at
#  http://gnuradio.org/doc/doxygen/classgr_1_1filter_1_1firdes.html

# use "window method" to design a low-pass FIR filter
#
# gain: overall gain of filter (typically 1.0)
# sampling_freq: sampling freq (Hz)
# cutoff_freq: center of transition band (Hz)
# transition_width: width of transition band (Hz).
#    The normalized width of the transition band is what sets the number of taps required. Narrow --> more taps
# window_type: What kind of window to use. Determines maximum attenuation and passband ripple.
# beta: parameter for Kaiser window

gain = 1
sampling_freq = 2.048e6
cutoff = 810e3
transition_width = 250e3

# Generate filter taps and print them out
taps = digital.filter.firdes_low_pass(gain, sampling_freq, cutoff, transition_width) # hamming window

print(len(taps))
for t in taps:
    print(t)
