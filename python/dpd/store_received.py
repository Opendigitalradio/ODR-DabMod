#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This is an example tool that shows how to connect to ODR-DabMod's dpd TCP server
# and get samples from there.
#
# Since the TX and RX samples are not perfectly aligned, the tool has to align them properly,
# which is done in two steps: First on sample-level using a correlation, then with subsample
# accuracy using a FFT approach.
#
# It requires SciPy and matplotlib.
#
# Copyright (C) 2017 Matthias P. Braendli
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import sys
import socket
import struct
import argparse
import os
import time
from src.GlobalConfig import GlobalConfig
from src.Measure import Measure

SIZEOF_SAMPLE = 8 # complex floats

parser = argparse.ArgumentParser(description="Plot the spectrum of ODR-DabMod's DPD feedback")
parser.add_argument('--samps', default='10240', type=int,
        help='Number of samples to request at once',
        required=False)
parser.add_argument('--port', default='50055', type=int,
        help='port to connect to ODR-DabMod DPD (default: 50055)',
        required=False)
parser.add_argument('--count', default='1', type=int,
        help='Number of recordings',
        required=False)
parser.add_argument('--verbose', type=int, default=0,
        help='Level of verbosity',
        required=False)
parser.add_argument('--plot',
                    help='Enable all plots, to be more selective choose plots in GlobalConfig.py',
                    action="store_true")
parser.add_argument('--samplerate', default=8192000, type=int,
                    help='Sample rate',
                    required=False)

cli_args = parser.parse_args()

cli_args.target_median = 0.05

c = GlobalConfig(cli_args, None)

meas = Measure(c, cli_args.samplerate, cli_args.port, cli_args.samps)

for i in range(int(cli_args.count)):
    if i>0:
        time.sleep(0.1)

    txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median = meas.get_samples()

    txframe_aligned.tofile("%d_tx_record.iq" % i)
    rxframe_aligned.tofile("%d_rx_record.iq" % i)

# The MIT License (MIT)
#
# Copyright (c) 2018 Matthias P. Braendli
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
