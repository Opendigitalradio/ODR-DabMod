#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""This Python script calculates and updates the parameter of the digital
predistortion module of the ODR-DabMod. More precisely the complex 
coefficients of the polynom which is used for predistortion."""

import logging
logging.basicConfig(format='%(asctime)s - %(module)s - %(levelname)s - %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S',
                    filename='/tmp/dpd.log',
                    filemode='w',
                    level=logging.DEBUG)

import src.Measure as Measure
import src.Model as Model
import src.Adapt as Adapt

port = 50055
port_rc = 9400
coef_path = "/home/andreas/dab/ODR-DabMod/polyCoefsCustom"
num_req = 10240

meas = Measure.Measure(port, num_req)
adapt = Adapt.Adapt(port_rc, coef_path)
coefs = adapt.get_coefs()
model = Model.Model(coefs)

txframe_aligned, rxframe_aligned = meas.get_samples()
coefs = model.get_next_coefs(txframe_aligned, rxframe_aligned)
adapt.set_coefs(coefs)

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
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
