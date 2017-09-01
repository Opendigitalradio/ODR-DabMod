# -*- coding: utf-8 -*-
#
# DPD Calculation Engine, test case for measure
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file
from unittest import TestCase
from Measure import Measure
import socket


class TestMeasure(TestCase):

    def _open_socks(self):
        sock_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock_server.bind(('localhost', 1234))
        sock_server.listen(1)

        sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_client.connect(('localhost', 1234))

        conn_server, addr_server = sock_server.accept()
        return conn_server, sock_client

    def test__recv_exact(self):
        m = Measure(1234, 1)
        payload = b"test payload"

        conn_server, sock_client = self._open_socks()
        conn_server.send(payload)
        rec = m._recv_exact(sock_client, len(payload))

        self.assertEqual(rec, payload,
                "Did not receive the same message as sended. (%s, %s)" %
                (rec, payload))

    def test_get_samples(self):
        self.fail()


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
