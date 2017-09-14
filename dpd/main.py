#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# DPD Calculation Engine main file.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

"""This Python script is the main file for ODR-DabMod's DPD Computation Engine.
This engine calculates and updates the parameter of the digital
predistortion module of ODR-DabMod."""

import datetime
import os
import time

import matplotlib
matplotlib.use('GTKAgg')

import logging

dt = datetime.datetime.now().isoformat()
logging_path = "/tmp/dpd_{}".format(dt).replace(".", "_").replace(":", "-")
os.makedirs(logging_path)
logging.basicConfig(format='%(asctime)s - %(module)s - %(levelname)s - %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S',
                    filename='{}/dpd.log'.format(logging_path),
                    filemode='w',
                    level=logging.DEBUG)

# also log up to INFO to console
console = logging.StreamHandler()
console.setLevel(logging.INFO)
# set a format which is simpler for console use
formatter = logging.Formatter('%(asctime)s - %(module)s - %(levelname)s - %(message)s')
# tell the handler to use this format
console.setFormatter(formatter)
# add the handler to the root logger
logging.getLogger('').addHandler(console)

import numpy as np
import traceback
import src.Measure as Measure
import src.Model as Model
import src.ExtractStatistic as ExtractStatistic
import src.Model_Poly
import src.Adapt as Adapt
import src.Agc as Agc
import src.TX_Agc as TX_Agc
import src.Symbol_align
import src.const
import src.MER
import argparse

parser = argparse.ArgumentParser(
    description="DPD Computation Engine for ODR-DabMod")
parser.add_argument('--port', default=50055, type=int,
                    help='port of DPD server to connect to (default: 50055)',
                    required=False)
parser.add_argument('--rc-port', default=9400, type=int,
                    help='port of ODR-DabMod ZMQ Remote Control to connect to (default: 9400)',
                    required=False)
parser.add_argument('--samplerate', default=8192000, type=int,
                    help='Sample rate',
                    required=False)
parser.add_argument('--coefs', default='poly.coef',
                    help='File with DPD coefficients, which will be read by ODR-DabMod',
                    required=False)
parser.add_argument('--txgain', default=73,
                    help='TX Gain',
                    required=False,
                    type=int)
parser.add_argument('--rxgain', default=30,
                    help='TX Gain',
                    required=False,
                    type=int)
parser.add_argument('--digital_gain', default=1,
                    help='Digital Gain',
                    required=False,
                    type=float)
parser.add_argument('--samps', default='81920', type=int,
                    help='Number of samples to request from ODR-DabMod',
                    required=False)
parser.add_argument('-i', '--iterations', default=1, type=int,
                    help='Number of iterations to run',
                    required=False)
parser.add_argument('-l', '--load-poly',
                    help='Load existing polynomial',
                    action="store_true")

cli_args = parser.parse_args()

port = cli_args.port
port_rc = cli_args.rc_port
coef_path = cli_args.coefs
digital_gain = cli_args.digital_gain
txgain = cli_args.txgain
rxgain = cli_args.rxgain
num_req = cli_args.samps
samplerate = cli_args.samplerate
num_iter = cli_args.iterations

SA = src.Symbol_align.Symbol_align(samplerate)
MER = src.MER.MER(samplerate)
c = src.const.const(samplerate)

meas = Measure.Measure(samplerate, port, num_req)
extStat = ExtractStatistic.ExtractStatistic(c, plot=True)
adapt = Adapt.Adapt(port_rc, coef_path)

coefs_am, coefs_pm = adapt.get_coefs()
model_poly = src.Model_Poly.Model_Poly(c, coefs_am, coefs_pm, plot=True)
if not cli_args.load_poly:
    coefs_am, coefs_pm = model_poly.get_default_coefs()

adapt.set_coefs(model_poly.coefs_am, model_poly.coefs_pm)
adapt.set_digital_gain(digital_gain)
adapt.set_txgain(txgain)
adapt.set_rxgain(rxgain)

tx_gain = adapt.get_txgain()
rx_gain = adapt.get_rxgain()
digital_gain = adapt.get_digital_gain()
dpd_coefs_am, dpd_coefs_pm = adapt.get_coefs()
logging.info(
    "TX gain {}, RX gain {}, dpd_coefs_am {},"
    " dpd_coefs_pm {}, digital_gain {}".format(
        tx_gain, rx_gain, dpd_coefs_am, dpd_coefs_pm, digital_gain
    )
)

tx_agc = TX_Agc.TX_Agc(adapt)

# Automatic Gain Control
agc = Agc.Agc(meas, adapt)
agc.run()

state = "measure"
i = 0
while i < num_iter:
    try:
        # Measure
        if state == "measure":
            txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median = meas.get_samples()
            tx, rx, phase_diff, n_per_bin = extStat.extract(txframe_aligned, rxframe_aligned)
            n_use = int(len(n_per_bin) * 0.6)
            tx = tx[:n_use]
            rx = rx[:n_use]
            phase_diff = phase_diff[:n_use]
            if all(c.ES_n_per_bin == np.array(n_per_bin)[0:n_use]):
                state = "model"
            else:
                state = "measure"

        # Model
        elif state == "model":
            coefs_am, coefs_pm = model_poly.get_next_coefs(tx, rx, phase_diff)
            del extStat
            extStat = ExtractStatistic.ExtractStatistic(c, plot=True)
            state = "adapt"

        # Adapt
        elif state == "adapt":
            adapt.set_coefs(coefs_am, coefs_pm)
            state = "report"
            i += 1

        # Report
        elif state == "report":
            try:
                off = SA.calc_offset(txframe_aligned)
                tx_mer = MER.calc_mer(txframe_aligned[off:off+c.T_U], debug=True)
                rx_mer = MER.calc_mer(rxframe_aligned[off:off+c.T_U], debug=True)
                mse = np.mean(np.abs((txframe_aligned - rxframe_aligned)**2))
                logging.info("It {}: TX_MER {}, RX_MER {}," \
                             " MSE {}, coefs_am {}, coefs_pm {}".
                             format(i, tx_mer, rx_mer, mse, coefs_am, coefs_pm))
                state = "measure"
            except:
                logging.warning("Iteration {}: Report failed.".format(i))
                logging.warning(traceback.format_exc())
                state = "measure"

    except Exception as e:
        logging.warning("Iteration {} failed.".format(i))
        logging.warning(traceback.format_exc())

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger, Matthias P. Braendli
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
