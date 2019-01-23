#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#   Copyright (C) 2018
#   Matthias P. Braendli, matthias.braendli@mpb.li
#
#    http://www.opendigitalradio.org
#
#   This file is part of ODR-DabMod.
#
#   ODR-DabMod is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as
#   published by the Free Software Foundation, either version 3 of the
#   License, or (at your option) any later version.
#
#   ODR-DabMod is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
import zmq
import json
from typing import List

class ModRemoteControl:
    """Interact with ODR-DabMod using the ZMQ RC"""
    def __init__(self, mod_host, mod_port=9400):
        self._host = mod_host
        self._port = mod_port
        self._ctx = zmq.Context()

    def _read(self, message_parts: List[str]):
        sock = zmq.Socket(self._ctx, zmq.REQ)
        sock.setsockopt(zmq.LINGER, 0)
        sock.connect("tcp://{}:{}".format(self._host, self._port))

        for i, part in enumerate(message_parts):
            if i == len(message_parts) - 1:
                f = 0
            else:
                f = zmq.SNDMORE
            sock.send(part.encode(), flags=f)

        # use poll for timeouts:
        poller = zmq.Poller()
        poller.register(sock, zmq.POLLIN)
        if poller.poll(5*1000): # 5s timeout in milliseconds
            recv = sock.recv_multipart()
            sock.close()
            return [r.decode() for r in recv]
        else:
            raise IOError("Timeout processing ZMQ request")

    def get_modules(self):
        modules = {}

        for mod in [json.loads(j) for j in self._read(['list'])]:
            params = {}
            pv_list = self._read(['show', mod['name']])

            for pv in pv_list:
                p, _, v = pv.partition(": ")
                params[p] = {"value": v.strip()}

            for p in mod['params']:
                if p in params:
                    params[p]["help"] = mod['params'][p]
            modules[mod['name']] = params

        return modules

    def get_param_value(self, module: str, param: str) -> str:
        value = self._read(['get', module, param])
        if value[0] == 'fail':
            raise ValueError("Error getting param: {}".format(value[1]))
        else:
            return value[0]

    def set_param_value(self, module: str, param: str, value: str) -> None:
        ret = self._read(['set', module, param, value])
        if ret[0] == 'fail':
            raise ValueError("Error setting param: {}".format(ret[1]))

