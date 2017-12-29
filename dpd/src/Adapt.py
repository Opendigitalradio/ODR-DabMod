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

import zmq
import logging
import numpy as np
import os
import datetime
import pickle

try:
    logging_path = os.path.dirname(logging.getLoggerClass().root.handlers[0].baseFilename)
except AttributeError:
    logging_path = None

LUT_LEN = 32
FORMAT_POLY = 1
FORMAT_LUT = 2


def _write_poly_coef_file(coefs_am, coefs_pm, path):
    assert (len(coefs_am) == len(coefs_pm))

    f = open(path, 'w')
    f.write("{}\n{}\n".format(FORMAT_POLY, len(coefs_am)))
    for coef in coefs_am:
        f.write("{}\n".format(coef))
    for coef in coefs_pm:
        f.write("{}\n".format(coef))
    f.close()


def _write_lut_file(scalefactor, lut, path):
    assert (len(lut) == LUT_LEN)

    f = open(path, 'w')
    f.write("{}\n{}\n".format(FORMAT_LUT, scalefactor))
    for coef in lut:
        f.write("{}\n{}\n".format(coef.real, coef.imag))
    f.close()


class Adapt:
    """Uses the ZMQ remote control to change parameters of the DabMod

    Parameters
    ----------
    port : int
        Port at which the ODR-DabMod is listening to connect the
        ZMQ remote control.
    """

    def __init__(self, port, coef_path):
        logging.debug("Instantiate Adapt object")
        self.port = port
        self.coef_path = coef_path
        self.host = "localhost"
        self._context = zmq.Context()

    def _connect(self):
        """Establish the connection to ODR-DabMod using
        a ZMQ socket that is in request mode (Client).
        Returns a socket"""
        sock = self._context.socket(zmq.REQ)
        poller = zmq.Poller()
        poller.register(sock, zmq.POLLIN)

        sock.connect("tcp://%s:%d" % (self.host, self.port))

        sock.send(b"ping")

        socks = dict(poller.poll(1000))
        if socks:
            if socks.get(sock) == zmq.POLLIN:
                data = [el.decode() for el in sock.recv_multipart()]

                if data != ['ok']:
                    raise RuntimeError(
                        "Invalid ZMQ RC answer to 'ping' at %s %d: %s" %
                        (self.host, self.port, data))
        else:
            sock.close(linger=10)
            raise RuntimeError(
                    "ZMQ RC does not respond to 'ping' at %s %d" %
                        (self.host, self.port))

        return sock

    def send_receive(self, message):
        """Send a message to ODR-DabMod. It always
        returns the answer ODR-DabMod sends back.

        An example message could be
        "get uhd txgain" or "set uhd txgain 50"

        Parameter
        ---------
        message : str
            The message string that will be sent to the receiver.
        """
        sock = self._connect()
        logging.debug("Send message: %s" % message)
        msg_parts = message.split(" ")
        for i, part in enumerate(msg_parts):
            if i == len(msg_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE

            sock.send(part.encode(), flags=f)

        data = [el.decode() for el in sock.recv_multipart()]
        logging.debug("Received message: %s" % message)
        return data

    def set_txgain(self, gain):
        """Set a new txgain for the ODR-DabMod.

        Parameters
        ----------
        gain : int
            new TX gain, in the same format as ODR-DabMod's config file
        """
        # TODO this is specific to the B200
        if gain < 0 or gain > 89:
            raise ValueError("Gain has to be in [0,89]")
        return self.send_receive("set uhd txgain %.4f" % float(gain))

    def get_txgain(self):
        """Get the txgain value in dB for the ODR-DabMod."""
        # TODO handle failure
        return float(self.send_receive("get uhd txgain")[0])

    def set_rxgain(self, gain):
        """Set a new rxgain for the ODR-DabMod.

        Parameters
        ----------
        gain : int
            new RX gain, in the same format as ODR-DabMod's config file
        """
        # TODO this is specific to the B200
        if gain < 0 or gain > 89:
            raise ValueError("Gain has to be in [0,89]")
        return self.send_receive("set uhd rxgain %.4f" % float(gain))

    def get_rxgain(self):
        """Get the rxgain value in dB for the ODR-DabMod."""
        # TODO handle failure
        return float(self.send_receive("get uhd rxgain")[0])

    def set_digital_gain(self, gain):
        """Set a new rxgain for the ODR-DabMod.

        Parameters
        ----------
        gain : int
            new RX gain, in the same format as ODR-DabMod's config file
        """
        msg = "set gain digital %.5f" % gain
        return self.send_receive(msg)

    def get_digital_gain(self):
        """Get the rxgain value in dB for the ODR-DabMod."""
        # TODO handle failure
        return float(self.send_receive("get gain digital")[0])

    def get_predistorter(self):
        """Load the coefficients from the file in the format given in the README,
        return ("poly", [AM coef], [PM coef]) or ("lut", scalefactor, [LUT entries])
        """
        f = open(self.coef_path, 'r')
        lines = f.readlines()
        predistorter_format = int(lines[0])
        if predistorter_format == FORMAT_POLY:
            coefs_am_out = []
            coefs_pm_out = []
            n_coefs = int(lines[1])
            coefs = [float(l) for l in lines[2:]]
            i = 0
            for c in coefs:
                if i < n_coefs:
                    coefs_am_out.append(c)
                elif i < 2 * n_coefs:
                    coefs_pm_out.append(c)
                else:
                    raise ValueError(
                        'Incorrect coef file format: too many'
                        ' coefficients in {}, should be {}, coefs are {}'
                            .format(self.coef_path, n_coefs, coefs))
                i += 1
            f.close()
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

    def set_predistorter(self, dpddata):
        """Update the predistorter data in the modulator. Takes the same
        tuple format as argument than the one returned get_predistorter()"""
        if dpddata[0] == "poly":
            coefs_am = dpddata[1]
            coefs_pm = dpddata[2]
            _write_poly_coef_file(coefs_am, coefs_pm, self.coef_path)
        elif dpddata[0] == "lut":
            scalefactor = dpddata[1]
            lut = dpddata[2]
            _write_lut_file(scalefactor, lut, self.coef_path)
        else:
            raise ValueError("Unknown predistorter '{}'".format(dpddata[0]))
        self.send_receive("set memlesspoly coeffile {}".format(self.coef_path))

    def dump(self, path=None):
        """Backup current settings to a file"""
        dt = datetime.datetime.now().isoformat()
        if path is None:
            if logging_path is not None:
                path = logging_path + "/" + dt + "_adapt.pkl"
            else:
                raise Exception("Cannot dump Adapt without either logging_path or path set")
        d = {
            "txgain": self.get_txgain(),
            "rxgain": self.get_rxgain(),
            "digital_gain": self.get_digital_gain(),
            "predistorter": self.get_predistorter()
        }
        with open(path, "w") as f:
            pickle.dump(d, f)

        return path

    def load(self, path):
        """Restore settings from a file"""
        with open(path, "r") as f:
            d = pickle.load(f)

        self.set_txgain(d["txgain"])
        self.set_digital_gain(d["digital_gain"])
        self.set_rxgain(d["rxgain"])
        self.set_predistorter(d["predistorter"])

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
