# -*- coding: utf-8 -*-
#
# DPD Computation Engine, constants and global configuration
#
# Source for DAB standard: etsi_EN_300_401_v010401p p145
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import numpy as np

class GlobalConfig:
    def __init__(self, samplerate: int, plot_location: str):
        self.sample_rate = samplerate
        assert self.sample_rate == 8192000, "We only support constants for 8192000 sample rate: {}".format(self.sample_rate)

        self.plot_location = plot_location
        plot = len(plot_location) > 0

        # DAB frame
        # Time domain
        oversample = int(self.sample_rate / 2048000)
        self.T_F = oversample * 196608  # Transmission frame duration
        self.T_NULL = oversample * 2656  # Null symbol duration
        self.T_S = oversample * 2552  # Duration of OFDM symbols of indices l = 1, 2, 3,... L;
        self.T_U = oversample * 2048  # Inverse of carrier spacing
        self.T_C = oversample * 504  # Duration of cyclic prefix

        self.median_to_peak = 12 # Estimated value for a DAB OFDM signal

        # Frequency Domain
        # example: np.delete(fft[3328:4865], 768)
        self.FFT_delta = 1536  # Number of carrier frequencies
        self.FFT_delete = 768
        self.FFT_start = 3328
        self.FFT_end = 4865

        # Calculate sample offset from phase rotation
        # time per sample = 1 / sample_rate
        # frequency per bin = 1kHz
        # phase difference per sample offset = delta_t * 2 * pi * delta_freq
        self.phase_offset_per_sample = 1. / self.sample_rate * 2 * np.pi * 1000

        # Constants for ExtractStatistic
        self.ES_end = 1.0
        self.ES_n_bins = 64
        self.ES_n_per_bin = 128  # Number of measurements pre bin

        # Constants for Measure_Shoulder
        self.MS_enable = False
        self.MS_plot = plot

        meas_offset = 976  # Offset from center frequency to measure shoulder [kHz]
        meas_width = 100  # Size of frequency delta to measure shoulder [kHz]
        shoulder_offset_edge = np.abs(meas_offset - self.FFT_delta)
        self.MS_shoulder_left_start = self.FFT_start - shoulder_offset_edge - meas_width / 2
        self.MS_shoulder_left_end = self.FFT_start - shoulder_offset_edge + meas_width / 2
        self.MS_shoulder_right_start = self.FFT_end + shoulder_offset_edge - meas_width / 2
        self.MS_shoulder_right_end = self.FFT_end + shoulder_offset_edge + meas_width / 2
        self.MS_peak_start = self.FFT_start + 100  # Ignore region near edges
        self.MS_peak_end = self.FFT_end - 100

        self.MS_FFT_size = 8192
        self.MS_averaging_size = 4 * self.MS_FFT_size
        self.MS_n_averaging = 40
        self.MS_n_proc = 4

        # Constants for MER
        self.MER_plot = plot

        # Constants for Model_PM
        # Set all phase offsets to zero for TX amplitude < MPM_tx_min
        self.MPM_tx_min = 0.1

        # Constants for RX_AGC
        self.RAGC_min_rxgain = 25  # USRP B200 specific
        self.RAGC_max_rxgain = 65  # USRP B200 specific
        self.RAGC_rx_median_target = 0.05

# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
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
