#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This example server simulates the ODR-DabMod's
# DPD server, taking samples from an IQ file
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

import sys
import socket
import struct
import argparse
import numpy as np
from datetime import datetime

SIZEOF_SAMPLE = 8 # complex floats
# Constants for TM 1
NbSymbols = 76
NbCarriers = 1536
Spacing = 2048
NullSize = 2656
SymSize = 2552
FicSizeOut = 288
FrameSize = NullSize + NbSymbols*SymSize

def main():
    parser = argparse.ArgumentParser(description="Simulate ODR-DabMod DPD server")
    parser.add_argument('--port', default='50055',
            help='port to listen on (default: 50055)',
            required=False)
    parser.add_argument('--file', help='I/Q File to read from (complex floats)',
            required=True)
    parser.add_argument('--samplerate', default='8192000', help='Sample rate',
            required=False)

    cli_args = parser.parse_args()

    serve_file(cli_args)

def recv_exact(sock, num_bytes):
    bufs = []
    while num_bytes > 0:
        b = sock.recv(num_bytes)
        if len(b) == 0:
            break
        num_bytes -= len(b)
        bufs.append(b)
    return b''.join(bufs)

def serve_file(options):
    oversampling = int(int(options.samplerate) / 2048000)
    consumesamples = 8*FrameSize * oversampling
    iq_data = np.fromfile(options.file, count=consumesamples, dtype=np.complex64)

    print("Loaded {} samples of IQ data".format(len(iq_data)))

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', int(options.port)))
    s.listen()

    try:
        while True:
            sock, addr_info = s.accept()
            print("Got a connection from {}".format(addr_info))

            ver = recv_exact(sock, 1)
            (num_samps,) = struct.unpack("=I", recv_exact(sock, 4))
            num_bytes = num_samps * SIZEOF_SAMPLE

            if num_bytes > len(iq_data):
                print("Truncating length to {} samples".format(len(iq_data)))
                num_samps = len(iq_data)
                num_bytes = num_samps * 4

            tx_sec = datetime.now().timestamp()
            tx_pps = int(16384000 * (tx_sec - int(tx_sec)))
            tx_second = int(tx_sec)

            # TX metadata and data
            sock.sendall(struct.pack("=III", num_samps, tx_second, tx_pps))
            sock.sendall(iq_data[-num_samps:].tobytes())

            # RX metadata and data
            rx_second = tx_second + 1
            rx_pps = tx_pps
            sock.sendall(struct.pack("=III", num_samps, rx_second, rx_pps))
            sock.sendall(iq_data[-num_samps:].tobytes())

            print("Sent {} samples".format(num_samps))

            sock.close()
    finally:
        s.close()
        raise

main()


# The MIT License (MIT)
#
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
