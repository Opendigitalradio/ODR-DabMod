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

import cherrypy
from cherrypy.lib.httputil import parse_query_string

import json
import urllib
import os

import io
import datetime
import threading

def send_ok(data=None):
    if data is not None:
        return {'status' : 'ok', 'data': data}
    else:
        return {'status': 'ok'}

def send_error(reason=""):
    if reason:
        return {'status' : 'error', 'reason': reason}
    else:
        return {'status' : 'error'}

class RXThread(threading.Thread):
    def __init__(self, api):
        super(RXThread, self).__init__()
        self.api = api
        self.running = False
        self.daemon = True

    def cancel(self):
        self.running = False

    def run(self):
        self.running = True
        while self.running:
            if self.api.dpd_pipe.poll(1):
                rx = self.api.dpd_pipe.recv()
                if rx['cmd'] == "quit":
                    break
                elif rx['cmd'] == "dpd-state":
                    self.api.dpd_state = rx['data']
                elif rx['cmd'] == "dpd-calibration-result":
                    self.api.calibration_result = rx['data']

class API:
    def __init__(self, mod_rc, dpd_pipe):
        self.mod_rc = mod_rc
        self.dpd_pipe = dpd_pipe
        self.dpd_state = None
        self.calibration_result = None
        self.receive_thread = RXThread(self)
        self.receive_thread.start()

    @cherrypy.expose
    def index(self):
        return """This is the api area."""

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def rc_parameters(self):
        return send_ok(self.mod_rc.get_modules())

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def parameter(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cl = cherrypy.request.headers['Content-Length']
            rawbody = cherrypy.request.body.read(int(cl))
            params = json.loads(rawbody.decode())
            try:
                self.mod_rc.set_param_value(params['controllable'], params['param'], params['value'])
            except ValueError as e:
                cherrypy.response.status = 400
                return send_error(str(e))
            return send_ok()
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def trigger_capture(self, **kwargs):
        if cherrypy.request.method == 'POST':
            self.dpd_pipe.send({'cmd': "dpd-capture"})
            return send_ok()
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_status(self, **kwargs):
        if self.dpd_state is not None:
            return send_ok(self.dpd_state)
        else:
            return send_error("DPD state unknown")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def calibrate(self, **kwargs):
        if cherrypy.request.method == 'POST':
            self.dpd_pipe.send({'cmd': "dpd-calibrate"})
            return send_ok()
        else:
            if self.calibration_result is not None:
                return send_ok(self.calibration_result)
            else:
                return send_error("DPD calibration result unknown")

