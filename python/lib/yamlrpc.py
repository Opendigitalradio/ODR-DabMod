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

"""yamlrpc is json-rpc, except that it's yaml and not json."""

# Same as jsonrpc version we're aiming to mirror in YAML
YAMLRPC_VERSION = "2.0"

import yaml
import socket
import struct

def request(request_id: int, method: str, params) -> bytes:
    r = {
            'yamlrpc': YAMLRPC_VERSION,
            'method': method,
            'params': params,
            'id': request_id}
    return yaml.dump(r).encode()

def response_success(request_id: int, result) -> bytes:
    r = {
            'yamlrpc': YAMLRPC_VERSION,
            'result': result,
            'error': None,
            'id': request_id}
    return yaml.dump(r).encode()

def response_error(request_id: int, error) -> bytes:
    r = {
            'yamlrpc': YAMLRPC_VERSION,
            'result': None,
            'error': error,
            'id': request_id}
    return yaml.dump(r).encode()

def notification(method: str, params) -> bytes:
    r = {
            'yamlrpc': YAMLRPC_VERSION,
            'method': method,
            'params': params}
    return yaml.dump(r).encode()

class Socket:
    def __init__(self, bind_port: int):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        if bind_port > 0:
            self.socket.bind(('127.0.0.1', bind_port))

    def receive_request(self):
        data, addr = self.socket.recvfrom(512)
        y = yaml.load(data.decode())

        if 'yamlrpc' not in y:
            raise ValueError("Message is not yamlrpc")
        if y['yamlrpc'] != YAMLRPC_VERSION:
            raise ValueError("Invalid yamlrpc version")

        # expect a request
        try:
            method = y['method']
            msg_id = y['id']
            params = y['params']
        except KeyError:
            raise ValueError("Incomplete message")
        return addr, msg_id, method, params

    def send_success_response(self, addr, msg_id: int, result):
        self.socket.sendto(response_success(msg_id, result), addr)

    def send_error_response(self, addr, msg_id: int, error):
        self.socket.sendto(response_error(msg_id, error), addr)

