#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# DPD Calculation Engine, apply stored configuration.
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

"""This Python script is the main file for ODR-DabMod's DPD Computation Engine.
This engine calculates and updates the parameter of the digital
predistortion module of ODR-DabMod."""

import datetime
import os
import matplotlib
import glob
import natsort
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

import src.Measure as Measure
import src.Model as Model
import src.ExtractStatistic as ExtractStatistic
import src.Adapt as Adapt
import src.RX_Agc as Agc
import src.TX_Agc as TX_Agc
import argparse

import src.Const
import src.Symbol_align
import src.Measure_Shoulders
import src.MER

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
parser.add_argument('--coefs', default='/tmp/poly.coef',
                    help='File with DPD coefficients, which will be read by ODR-DabMod',
                    required=False)
parser.add_argument('--txgain', default=75,
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
parser.add_argument('--target_median', default=0.1,
                    help='target_median',
                    required=False,
                    type=float)
parser.add_argument('--searchpath', default='./stored', type=str,
                    help='Path to search .pkl files with stored configuration'
                         'for adapt',
                    required=False)
parser.add_argument('-L', '--lut',
                    help='Use lookup table instead of polynomial predistorter',
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
searchpath = cli_args.searchpath
target_median = cli_args.target_median

c = src.Const.Const(samplerate, target_median, False)
SA = src.Symbol_align.Symbol_align(c)
MER = src.MER.MER(c)
MS = src.Measure_Shoulders.Measure_Shoulders(c)

meas = Measure.Measure(samplerate, port, num_req)
extStat = ExtractStatistic.ExtractStatistic(c)
adapt = Adapt.Adapt(port_rc, coef_path)
dpddata = adapt.get_predistorter

if cli_args.lut:
    model = Model.Lut(c)
else:
    model = Model.Poly(c)
adapt.set_predistorter(model.get_dpd_data())
adapt.set_digital_gain(digital_gain)
adapt.set_txgain(txgain)
adapt.set_rxgain(rxgain)

tx_gain = adapt.get_txgain()
rx_gain = adapt.get_rxgain()
digital_gain = adapt.get_digital_gain()

dpddata = adapt.get_predistorter()
if dpddata[0] == "poly":
    coefs_am = dpddata[1]
    coefs_pm = dpddata[2]
    logging.info(
        "TX gain {}, RX gain {}, dpd_coefs_am {},"
        " dpd_coefs_pm {}, digital_gain {}".format(
            tx_gain, rx_gain, coefs_am, coefs_pm, digital_gain
        )
    )
elif dpddata[0] == "lut":
    scalefactor = dpddata[1]
    lut = dpddata[2]
    logging.info(
        "TX gain {}, RX gain {}, LUT scalefactor {},"
        " LUT {}, digital_gain {}".format(
            tx_gain, rx_gain, scalefactor, lut, digital_gain
        )
    )
else:
    logging.error("Unknown dpd data format {}".format(dpddata[0]))

tx_agc = TX_Agc.TX_Agc(adapt, c)

# Automatic Gain Control
agc = Agc.Agc(meas, adapt, c)
agc.run()

paths = natsort.natsorted(glob.glob(searchpath + "/*.pkl"))
print(paths)

for i, path in enumerate(paths):
    print(i, path)
    adapt.load(path)
    dpddata_after = adapt.get_predistorter()

    coefs_am, coefs_pm = model.reset_coefs()
    adapt.set_predistorter(("poly", coefs_am, coefs_pm))
    print("Loaded configuration without pre-distortion")

    raw_input("Key for pre-distortion ")
    adapt.set_predistorter(dpddata_after)
    print("Pre-distortion done")

    raw_input("Key for next ")

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
