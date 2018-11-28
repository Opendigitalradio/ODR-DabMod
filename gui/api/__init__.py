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
from cherrypy.process import wspbus, plugins
from cherrypy.lib.httputil import parse_query_string

import urllib
import os

import io
import datetime

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

class API(plugins.SimplePlugin):
    def __init__(self, mod_rc, bus):
        plugins.SimplePlugin.__init__(self, bus)
        self.mod_rc = mod_rc
        self.dpd_state = None
        self.calibration_result = None

    def start(self):
        self.bus.subscribe("dpd-state", self.dpd_state)
        self.bus.subscribe("dpd-calibration-result", self.calibration_result)

    def stop(self):
        self.bus.unsubscribe("dpd-state", self.dpd_state)
        self.bus.unsubscribe("dpd-calibration-result", self.calibration_result)

    def calibration_result(self, new_result):
        self.calibration_result = new_result

    def dpd_state(self, new_state):
        self.dpd_state = new_state

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
            params = json.loads(rawbody)
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
            cherrypy.engine.publish('dpd-capture', None)
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
    def dpd_calibrate(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cherrypy.engine.publish('dpd-calibrate', None)
            return send_ok()
        else:
            if self.dpd_state is not None:
                return send_ok(self.calibration_result)
            else:
                return send_error("DPD calibration result unknown")

