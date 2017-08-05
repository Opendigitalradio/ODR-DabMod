# -*- coding: utf-8 -*-
"""
This module is used to change settings of ODR-DabMod using
the ZMQ remote control socket.
"""

import zmq
import exceptions
import logging
import numpy as np

port = 9400

class Adapt:
    """Uses the ZMQ remote control to change parameters 
    of the DabMod

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
        self._connect()

    def _connect(self):
        """Establish the connection to ODR-DabMod using
        a ZMQ socket that is in request mode (Client)"""
        context = zmq.Context()
        sock = context.socket(zmq.REQ)
        sock.connect("tcp://%s:%d" % (self.host, self.port))

        sock.send(b"ping")
        data = sock.recv_multipart()

        if data != ['ok']:
            raise exceptions.RuntimeError(
                "Could not connect to server %s %d." %
                (self.host, self.port))

        self.sock = sock

    def send_receive(self, message):
        """Used to send a message to the ODR-DabMod. It always
        returns a answer it also receives the next message
        from ODR-DabMod over the ZMQ remote control socket.

        Parameter
        ---------
        message : str
            The message string that will be sent to 
            the receiver.
        """
        logging.info("Send message: %s" % message)
        msg_parts = message.split(" ")
        for i, part in enumerate(msg_parts):
            if i == len(msg_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE

            self.sock.send(part.encode(), flags=f)

        data = self.sock.recv_multipart()
        logging.info("Received message: %s" % message)
        return data

    def set_txgain(self, gain):
        """Set a new txgain for the ORD-DabMod.

        Parameters
        ----------
        gain : int
            Value that will be set to be txgain
        """
        if gain < 0 or gain > 89:
            raise exceptions.ValueError("Gain has to be in [0,89]")
        return self.send_receive("set uhd txgain %d" % gain)

    def get_txgain(self):
        """Get the txgain value in dB for the ORD-DabMod."""
        return self.send_receive("get uhd txgain")

    def set_rxgain(self, gain):
        """Set a new rxgain for the ORD-DabMod.

        Parameters
        ----------
        gain : int
            Value that will be set to be rxgain
        """
        if gain < 0 or gain > 89:
            raise exceptions.ValueError("Gain has to be in [0,89]")
        return self.send_receive("set uhd rxgain %d" % gain)

    def get_rxgain(self):
        """Get the rxgain value in dB for the ORD-DabMod."""
        return self.send_receive("get uhd rxgain")

    def _read_coef_file(self):
        coefs_complex = []
        f = open(self.coef_path, 'r')
        lines = f.readlines()
        n_coefs = lines[0]
        coefs = [float(l) for l in lines[1:]]
        for r, c in zip(coefs[0::2], coefs[1::2]):
            coefs_complex.append(np.complex64(r + 1j * c))
        f.close()
        return coefs_complex

    def get_coefs(self):
        return self._read_coef_file()

    def _write_coef_file(self, coefs_complex):
        coef_path = "/home/andreas/dab/ODR-DabMod/polyCoefsCustom"
        f = open(coef_path, 'w')
        f.write(str(len(coefs_complex)) + "\n")
        for coef in coefs_complex:
            f.write(str(coef.real) + "\n")
            f.write(str(coef.imag) + "\n")
        f.close()

    def set_coefs(self, coefs_complex):
        self._write_coef_file(coefs_complex)
        self.send_receive("set memlesspoly coeffile polyCoefsCustom")

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
