# -*- coding: utf-8 -*-
#
# Modulation Error Rate
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import datetime
import os
import logging
import time
try:
    logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)
except:
    logging_path = "/tmp/"

import src.Const
import src.Dab_Util
import numpy as np
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt


class Test_data:
    def __init__(self, sample_rate, type):
        """
        Standardized access to complex64 test data files.
        
        :param sample_rate: 
        :param type: 
        
        Testing:
        TD = src.Test_data.Test_data(8192000, 'file')
        tx_orig = TD.get_symbol(0,0)
        fig = plt.figure(figsize=(9,6))
        ax = fig.add_subplot(2,1,1)
        ax.plot(tx_orig)
        ax = fig.add_subplot(2,1,2)
        plt.plot(np.angle(np.fft.fftshift(np.fft.fft(tx_orig))), 'p')
        """

        self.c = src.Const.Const(sample_rate,, False
        self.du = src.Dab_Util.Dab_Util(sample_rate)

        self.file_paths = {
            (2048000, 'file'):
                ("./test_data/odr-dabmod_to_file_2048_NoFir_noDPD.iq",
                 (
                    self.c.T_F +    # Pipelineing
                    self.c.T_NULL + # NULL Symbol
                    self.c.T_S +    # Synchronization Symbol
                    self.c.T_C      # Cyclic Prefix
                 )),
            (8192000, 'file'):
                ("./test_data/odr-dabmod_to_file_8192_NoFir_noDPD.iq",
                 (
                     self.c.T_F +    # Pipelining
                     self.c.T_U +    # Pipelining Resampler TODO(?)
                     self.c.T_NULL + # NULL Symbol
                     self.c.T_S +    # Synchronization Symbol
                     self.c.T_C      # Cyclic Prefix
                 )),
            (8192000, 'rec_noFir'):
                ("./test_data/odr-dabmod_reconding_8192_NoFir_DPD_2104.iq",
                 ( 64 )),
            (8192000, 'rec_fir'):
                ("./test_data/odr-dabmod_reconding_8192_Fir_DPD_2104.iq",
                 ( 232 )),
        }

        config = (sample_rate, type)
        if not config in self.file_paths.keys():
            raise RuntimeError("Configuration not found, possible are:\n {}".
                               format(self.file_paths))

        self.path, self.file_offset = self.file_paths[(sample_rate, type)]

    def _load_from_file(self, offset, length):
        print(offset, length, self.file_offset)
        return self.du.fromfile(
            self.path,
            length=length,
            offset=offset + self.file_offset)

    def get_symbol_without_prefix(self,
                                  frame_idx=0,
                                  symbol_idx=0,
                                  off=0):
        return self._load_from_file(
            frame_idx*self.c.T_F +
            symbol_idx*self.c.T_S +
            off,
            self.c.T_U)

    def get_symbol_with_prefix(self,
                               frame_idx=0,
                               symbol_idx=0,
                               n_symbols=1,
                               off=0):
        offset = (
            frame_idx*self.c.T_F +
            symbol_idx*self.c.T_S -
            self.c.T_C +
            off)
        return self._load_from_file( offset, self.c.T_S * n_symbols)

    def get_file_length_in_symbols(self):
        symbol_size = float(
                        64/8 * # complex 64
                        self.c.T_S # Symbol Size
                    )
        return os.path.getsize(self.path) / symbol_size


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
