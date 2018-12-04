#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# DPD Computation Engine standalone main file.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

"""This Python script is the main file for ODR-DabMod's DPD Computation Engine running
in stand-alone mode.

This engine calculates and updates the parameter of the digital
predistortion module of ODR-DabMod."""

import sys
import datetime
import os
import argparse
import matplotlib

matplotlib.use('Agg')

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
parser.add_argument('--txgain', default=-1,
                    help='TX Gain, -1 to leave unchanged',
                    required=False,
                    type=int)
parser.add_argument('--rxgain', default=30,
                    help='TX Gain, -1 to leave unchanged',
                    required=False,
                    type=int)
parser.add_argument('--digital_gain', default=0.01,
                    help='Digital Gain',
                    required=False,
                    type=float)
parser.add_argument('--target_median', default=0.05,
                    help='The target median for the RX and TX AGC',
                    required=False,
                    type=float)
parser.add_argument('--samps', default='81920', type=int,
                    help='Number of samples to request from ODR-DabMod',
                    required=False)
parser.add_argument('-i', '--iterations', default=10, type=int,
                    help='Number of iterations to run',
                    required=False)
parser.add_argument('-L', '--lut',
                    help='Use lookup table instead of polynomial predistorter',
                    action="store_true")
parser.add_argument('--enable-txgain-agc',
                    help='Enable the TX gain AGC',
                    action="store_true")
parser.add_argument('--plot',
                    help='Enable all plots, to be more selective choose plots in GlobalConfig.py',
                    action="store_true")
parser.add_argument('--name', default="", type=str,
                    help='Name of the logging directory')
parser.add_argument('-r', '--reset', action="store_true",
                    help='Reset the DPD settings to the defaults.')
parser.add_argument('-s', '--status', action="store_true",
                    help='Display the currently running DPD settings.')
parser.add_argument('--measure', action="store_true",
                    help='Only measure metrics once')

cli_args = parser.parse_args()

port = cli_args.port
port_rc = cli_args.rc_port
coef_path = cli_args.coefs
digital_gain = cli_args.digital_gain
num_iter = cli_args.iterations
rxgain = cli_args.rxgain
txgain = cli_args.txgain
name = cli_args.name
plot = cli_args.plot

# Logging
import logging

# Simple usage scenarios don't need to clutter /tmp
if not (cli_args.status or cli_args.reset or cli_args.measure):
    dt = datetime.datetime.now().isoformat()
    logging_path = '/tmp/dpd_{}'.format(dt).replace('.', '_').replace(':', '-')
    if name:
        logging_path += '_' + name
    print("Logs and plots written to {}".format(logging_path))
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
else:
    dt = datetime.datetime.now().isoformat()
    logging.basicConfig(format='%(asctime)s - %(module)s - %(levelname)s - %(message)s',
                        datefmt='%Y-%m-%d %H:%M:%S',
                        level=logging.INFO)
    logging_path = None

logging.info("DPDCE starting up with options: {}".format(cli_args))

import numpy as np
import traceback
from src.Model import Lut, Poly
import src.Heuristics as Heuristics
from src.Measure import Measure
from src.ExtractStatistic import ExtractStatistic
from src.Adapt import Adapt, dpddata_to_str
from src.RX_Agc import Agc
from src.TX_Agc import TX_Agc
from src.Symbol_align import Symbol_align
from src.GlobalConfig import GlobalConfig
from src.MER import MER
from src.Measure_Shoulders import Measure_Shoulders

c = GlobalConfig(cli_args, logging_path)
SA = Symbol_align(c)
MER = MER(c)
MS = Measure_Shoulders(c)
meas = Measure(c, cli_args.samplerate, port, cli_args.samps)
extStat = ExtractStatistic(c)
adapt = Adapt(c, port_rc, coef_path)

if cli_args.status:
    txgain = adapt.get_txgain()
    rxgain = adapt.get_rxgain()
    digital_gain = adapt.get_digital_gain()
    dpddata = dpddata_to_str(adapt.get_predistorter())

    logging.info("ODR-DabMod currently running with TXGain {}, RXGain {}, digital gain {} and {}".format(
        txgain, rxgain, digital_gain, dpddata))
    sys.exit(0)

if cli_args.lut:
    model = Lut(c)
else:
    model = Poly(c)

# Models have the default settings on startup
adapt.set_predistorter(model.get_dpd_data())
adapt.set_digital_gain(digital_gain)

# Set RX Gain
if rxgain == -1:
    rxgain = adapt.get_rxgain()
else:
    adapt.set_rxgain(rxgain)

# Set TX Gain
if txgain == -1:
    txgain = adapt.get_txgain()
else:
    adapt.set_txgain(txgain)

tx_gain = adapt.get_txgain()
rx_gain = adapt.get_rxgain()
digital_gain = adapt.get_digital_gain()

dpddata = adapt.get_predistorter()

logging.info("TX gain {}, RX gain {}, digital_gain {}, {!s}".format(
        tx_gain, rx_gain, digital_gain, dpddata_to_str(dpddata)))

if cli_args.reset:
    logging.info("DPD Settings were reset to default values.")
    sys.exit(0)

# Automatic Gain Control
agc = Agc(meas, adapt, c)
agc.run()

if cli_args.measure:
    txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median = meas.get_samples()

    print("TX signal median {}".format(np.median(np.abs(txframe_aligned))))
    print("RX signal median {}".format(rx_median))

    tx, rx, phase_diff, n_per_bin = extStat.extract(txframe_aligned, rxframe_aligned)

    off = SA.calc_offset(txframe_aligned)
    print("off {}".format(off))
    tx_mer = MER.calc_mer(txframe_aligned[off:off + c.T_U], debug_name='TX')
    print("tx_mer {}".format(tx_mer))
    rx_mer = MER.calc_mer(rxframe_aligned[off:off + c.T_U], debug_name='RX')
    print("rx_mer {}".format(rx_mer))

    mse = np.mean(np.abs((txframe_aligned - rxframe_aligned) ** 2))
    print("mse {}".format(mse))

    digital_gain = adapt.get_digital_gain()
    print("digital_gain {}".format(digital_gain))

    #rx_shoulder_tuple = MS.average_shoulders(rxframe_aligned)
    #tx_shoulder_tuple = MS.average_shoulders(txframe_aligned)
    sys.exit(0)

# Disable TXGain AGC by default, so that the experiments are controlled
# better.
tx_agc = None
if cli_args.enable_txgain_agc:
    tx_agc = TX_Agc(adapt, c)

state = 'report'
i = 0
lr = None
n_meas = None
while i < num_iter:
    try:
        # Measure
        if state == 'measure':
            # Get Samples and check gain
            txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median = meas.get_samples()
            if tx_agc and tx_agc.adapt_if_necessary(txframe_aligned):
                continue

            # Extract usable data from measurement
            tx, rx, phase_diff, n_per_bin = extStat.extract(txframe_aligned, rxframe_aligned)

            n_meas = Heuristics.get_n_meas(i)
            if extStat.n_meas >= n_meas:  # Use as many measurements nr of runs
                state = 'model'
            else:
                state = 'measure'

        # Model
        elif state == 'model':
            # Calculate new model parameters and delete old measurements
            if any([x is None for x in [tx, rx, phase_diff]]):
                logging.error("No data to calculate model")
                state = 'measure'
                continue

            lr = Heuristics.get_learning_rate(i)
            model.train(tx, rx, phase_diff, lr=lr)
            dpddata = model.get_dpd_data()
            extStat = ExtractStatistic(c)
            state = 'adapt'

        # Adapt
        elif state == 'adapt':
            adapt.set_predistorter(dpddata)
            state = 'report'

        # Report
        elif state == 'report':
            try:
                txframe_aligned, tx_ts, rxframe_aligned, rx_ts, rx_median = meas.get_samples()

                # Store all settings for pre-distortion, tx and rx
                adapt.dump()

                # Collect logging data
                off = SA.calc_offset(txframe_aligned)
                tx_mer = MER.calc_mer(txframe_aligned[off:off + c.T_U], debug_name='TX')
                rx_mer = MER.calc_mer(rxframe_aligned[off:off + c.T_U], debug_name='RX')
                mse = np.mean(np.abs((txframe_aligned - rxframe_aligned) ** 2))
                tx_gain = adapt.get_txgain()
                rx_gain = adapt.get_rxgain()
                digital_gain = adapt.get_digital_gain()
                tx_median = np.median(np.abs(txframe_aligned))
                rx_shoulder_tuple = MS.average_shoulders(rxframe_aligned)
                tx_shoulder_tuple = MS.average_shoulders(txframe_aligned)

                # Generic logging
                logging.info(list((name, eval(name)) for name in
                                  ['i', 'tx_mer', 'tx_shoulder_tuple', 'rx_mer',
                                   'rx_shoulder_tuple', 'mse', 'tx_gain',
                                   'digital_gain', 'rx_gain', 'rx_median',
                                   'tx_median', 'lr', 'n_meas']))

                # Model specific logging
                if dpddata[0] == 'poly':
                    coefs_am = dpddata[1]
                    coefs_pm = dpddata[2]
                    logging.info('It {}: coefs_am {}'.
                                 format(i, coefs_am))
                    logging.info('It {}: coefs_pm {}'.
                                 format(i, coefs_pm))
                elif dpddata[0] == 'lut':
                    scalefactor = dpddata[1]
                    lut = dpddata[2]
                    logging.info('It {}: LUT scalefactor {}, LUT {}'.
                                 format(i, scalefactor, lut))
            except:
                logging.error('Iteration {}: Report failed.'.format(i))
                logging.error(traceback.format_exc())
            i += 1
            state = 'measure'

    except:
        logging.error('Iteration {} failed.'.format(i))
        logging.error(traceback.format_exc())
        i += 1
        state = 'measure'

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
# Copyright (c) 2017 Matthias P. Braendli
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
