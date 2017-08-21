# -*- coding: utf-8 -*-
#
# DPD Calculation Engine: updates ODR-DabMod's predistortion block.
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

class Adapt:
    """Uses the ZMQ remote control to change parameters of the DabMod

    Parameters
    ----------
    port : int
        Port at which the ODR-DabMod is listening to connect the
        ZMQ remote control.
    """

    def __init__(self, port, coef_path):
        logging.info("Instantiate Adapt object")
        self.port = port
        self.coef_path = coef_path
        self.host = "localhost"
        self._context = zmq.Context()

    def _connect(self):
        """Establish the connection to ODR-DabMod using
        a ZMQ socket that is in request mode (Client).
        Returns a socket"""
        sock = self._context.socket(zmq.REQ)
        sock.connect("tcp://%s:%d" % (self.host, self.port))

        sock.send(b"ping")
        data = [el.decode() for el in sock.recv_multipart()]

        if data != ['ok']:
            raise RuntimeError(
                    "Could not ping server at %s %d: %s" %
                    (self.host, self.port, data))

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
        logging.info("Send message: %s" % message)
        msg_parts = message.split(" ")
        for i, part in enumerate(msg_parts):
            if i == len(msg_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE

            sock.send(part.encode(), flags=f)

        data = [el.decode() for el in sock.recv_multipart()]
        logging.info("Received message: %s" % message)
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
        return self.send_receive("set uhd txgain %d" % gain)

    def get_txgain(self):
        """Get the txgain value in dB for the ODR-DabMod."""
        # TODO handle failure
        return self.send_receive("get uhd txgain")

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
        return self.send_receive("set uhd rxgain %d" % gain)

    def get_rxgain(self):
        """Get the rxgain value in dB for the ODR-DabMod."""
        # TODO handle failure
        return self.send_receive("get uhd rxgain")

    def _read_coef_file(self):
        """Load the coefficients from the file in the format given in the README"""
        coefs_out = []
        f = open(self.coef_path, 'r')
        lines = f.readlines()
        n_coefs = int(lines[0])
        coefs = [float(l) for l in lines[1:]]
        i = 0
        for c in coefs:
            if i < n_coefs:
                coefs_out.append(c)
            else:
                raise ValueError(
                    "Incorrect coef file format: too many coefficients in {}, should be {}, coefs are {}"
                        .format(self.coef_path, n_coefs, coefs))
            i += 1
        f.close()
        return coefs_out

    def get_coefs(self):
        return self._read_coef_file()

    def _write_coef_file(self, coefs):
        f = open(self.coef_path, 'w')
        f.write("{}\n".format(len(coefs)))
        for coef in coefs:
            f.write("{}\n".format(coef))
        f.close()

    def set_coefs(self, coefs):
        self._write_coef_file(coefs)
        self.send_receive("set memlesspoly coeffile {}".format(self.coef_path))

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
