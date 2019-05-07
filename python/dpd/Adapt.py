# -*- coding: utf-8 -*-
#
# DPD Computation Engine: updates ODR-DabMod's settings
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file
"""
This module is used to change settings of ODR-DabMod using
the ZMQ remote control socket.
"""

import logging
import numpy as np
import os.path
import pickle
from lib import zmqrc
from typing import List

LUT_LEN = 32
FORMAT_POLY = 1
FORMAT_LUT = 2


def _write_poly_coef_file(coefs_am: List[float], coefs_pm: List[float], path: str) -> None:
    assert len(coefs_am) == len(coefs_pm)

    with open(path, 'w') as f:
        f.write("{}\n{}\n".format(FORMAT_POLY, len(coefs_am)))
        for coef in coefs_am:
            f.write("{}\n".format(coef))
        for coef in coefs_pm:
            f.write("{}\n".format(coef))


def _write_lut_file(scalefactor: float, lut: List[complex], path: str) -> None:
    assert len(lut) == LUT_LEN

    with open(path, 'w') as f:
        f.write("{}\n{}\n".format(FORMAT_LUT, scalefactor))
        for coef in lut:
            f.write("{}\n{}\n".format(coef.real, coef.imag))

def dpddata_to_str(dpddata) -> str:
    if dpddata[0] == "poly":
        coefs_am = dpddata[1]
        coefs_pm = dpddata[2]
        return "Poly: AM/AM {}, AM/PM {}".format(
                coefs_am, coefs_pm)
    elif dpddata[0] == "lut":
        scalefactor = dpddata[1]
        lut = dpddata[2]
        return "LUT: scalefactor {}, LUT {}".format(
                scalefactor, lut)
    else:
        raise ValueError("Unknown dpddata type {}".format(dpddata[0]))

class Adapt:
    """Uses the ZMQ remote control to change parameters of the DabMod """

    def __init__(self, port: int, coef_path: str, plot_location: str):
        logging.debug("Instantiate Adapt object")
        self._port = port
        self._coef_path = coef_path
        self._plot_location = plot_location
        self._host = "localhost"
        self._mod_rc = zmqrc.ModRemoteControl(self._host, self._port)

    def set_txgain(self, gain : float) -> None:
        # TODO this is specific to the B200
        if gain < 0 or gain > 89:
            raise ValueError("Gain has to be in [0,89]")
        self._mod_rc.set_param_value("sdr", "txgain", "%.4f" % float(gain))

    def get_txgain(self) -> float:
        """Get the txgain value in dB, or -1 in case of error"""
        try:
            return float(self._mod_rc.get_param_value("sdr", "txgain"))
        except ValueError as e:
            logging.warning("Adapt: get_txgain error: {}".format(e))
            return -1.0

    def set_rxgain(self, gain: float) -> None:
        # TODO this is specific to the B200
        if gain < 0 or gain > 89:
            raise ValueError("Gain has to be in [0,89]")
        self._mod_rc.set_param_value("sdr", "rxgain", "%.4f" % float(gain))

    def get_rxgain(self) -> float:
        """Get the rxgain value in dB, or -1 in case of error"""
        try:
            return float(self._mod_rc.get_param_value("sdr", "rxgain"))
        except ValueError as e:
            logging.warning("Adapt: get_rxgain error: {}".format(e))
            return -1.0

    def set_digital_gain(self, gain: float) -> None:
        self._mod_rc.set_param_value("gain", "digital", "%.5f" % float(gain))

    def get_digital_gain(self) -> float:
        """Get the digital gain value in linear scale, or -1 in case
        of error"""
        try:
            return float(self._mod_rc.get_param_value("gain", "digital"))
        except ValueError as e:
            logging.warning("Adapt: get_digital_gain error: {}".format(e))
            return -1.0

    def get_predistorter(self):
        """Load the coefficients from the file in the format given in the README,
        return ("poly", [AM coef], [PM coef]) or ("lut", scalefactor, [LUT entries])
        """
        with open(self._coef_path, 'r') as f:
            lines = f.readlines()
            predistorter_format = int(lines[0])
            if predistorter_format == FORMAT_POLY:
                coefs_am_out = []
                coefs_pm_out = []
                n_coefs = int(lines[1])
                coefs = [float(l) for l in lines[2:]]
                for i, c in enumerate(coefs):
                    if i < n_coefs:
                        coefs_am_out.append(c)
                    elif i < 2 * n_coefs:
                        coefs_pm_out.append(c)
                    else:
                        raise ValueError(
                            'Incorrect coef file format: too many'
                            ' coefficients in {}, should be {}, coefs are {}'
                                .format(self._coef_path, n_coefs, coefs))
                return 'poly', coefs_am_out, coefs_pm_out
            elif predistorter_format == FORMAT_LUT:
                scalefactor = int(lines[1])
                coefs = np.array([float(l) for l in lines[2:]], dtype=np.float32)
                coefs = coefs.reshape((-1, 2))
                lut = coefs[..., 0] + 1j * coefs[..., 1]
                if len(lut) != LUT_LEN:
                    raise ValueError("Incorrect number of LUT entries ({} expected {})".format(len(lut), LUT_LEN))
                return 'lut', scalefactor, lut
            else:
                raise ValueError("Unknown predistorter format {}".format(predistorter_format))

    def set_predistorter(self, dpddata) -> None:
        """Update the predistorter data in the modulator. Takes the same
        tuple format as argument than the one returned get_predistorter()"""
        if dpddata[0] == "poly":
            coefs_am = dpddata[1]
            coefs_pm = dpddata[2]
            _write_poly_coef_file(coefs_am, coefs_pm, self._coef_path)
        elif dpddata[0] == "lut":
            scalefactor = dpddata[1]
            lut = dpddata[2]
            _write_lut_file(scalefactor, lut, self._coef_path)
        else:
            raise ValueError("Unknown predistorter '{}'".format(dpddata[0]))
        self._mod_rc.set_param_value("memlesspoly", "coeffile", self._coef_path)

    def dump(self, path: str) -> None:
        """Backup current settings to a file"""

        d = {
            "txgain": self.get_txgain(),
            "rxgain": self.get_rxgain(),
            "digital_gain": self.get_digital_gain(),
            "dpddata": self.get_predistorter()
        }

        with open(path, "wb") as f:
            pickle.dump(d, f)

    def restore(self, path: str):
        """Restore settings from a file"""
        with open(path, "rb") as f:
            d = pickle.load(f)

            self.set_txgain(0)

            # If any of the following fail, we will be running
            # with the safe value of txgain=0
            self.set_digital_gain(d["digital_gain"])
            self.set_rxgain(d["rxgain"])
            self.set_predistorter(d["dpddata"])
            self.set_txgain(d["txgain"])

            return d

# The MIT License (MIT)
#
# Copyright (c) 2019 Matthias P. Braendli
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
