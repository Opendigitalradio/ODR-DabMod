# -*- coding: utf-8 -*-
#
# DPD Computation Engine, model implementation using polynomial
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import os
import logging
import numpy as np

class Lut:
    """Implements a model that calculates lookup table coefficients"""

    def __init__(self,
                 c,
                 learning_rate=1.,
                 plot=False):
        """

        :rtype: 
        """
        logging.debug("Initialising LUT Model")
        self.c = c
        self.learning_rate = learning_rate
        self.plot = plot
        self.reset_coefs()

    def reset_coefs(self):
        self.scalefactor = 0xFFFFFFFF  # max uint32_t value
        self.lut = np.ones(32, dtype=np.complex64)

    def train(self, tx_abs, rx_abs, phase_diff):
        pass

    def get_dpd_data(self):
        return "lut", self.scalefactor, self.lut

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
