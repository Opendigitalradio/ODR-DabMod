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

import json
import cherrypy
from cherrypy.lib.httputil import parse_query_string

import urllib
import os

import io
import datetime

def send_ok(data):
    return json.dumps({'status' : 'ok', 'data': data}).encode()

def send_error(reason=""):
    return json.dumps({'status' : 'error', 'reason': reason}).encode()

class API:
    def __init__(self, mod_rc, dpd):
        self.mod_rc = mod_rc
        self.dpd = dpd

    @cherrypy.expose
    def index(self):
        return """This is the api area."""

    @cherrypy.expose
    def rc_parameters(self):
        cherrypy.response.headers["Content-Type"] = "application/json"
        return send_ok(self.mod_rc.get_modules())

    @cherrypy.expose
    def parameter(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cherrypy.response.headers["Content-Type"] = "application/json"
            cl = cherrypy.request.headers['Content-Length']
            rawbody = cherrypy.request.body.read(int(cl))
            params = json.loads(rawbody)
            try:
                self.mod_rc.set_param_value(params['controllable'], params['param'], params['value'])
            except ValueError as e:
                cherrypy.response.status = 400
                return send_error(str(e))
            return send_ok(None)
        else:
            cherrypy.response.headers["Content-Type"] = "application/json"
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    def trigger_capture(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cherrypy.response.headers["Content-Type"] = "application/json"
            try:
                return send_ok(self.dpd.capture_samples())
            except ValueError as e:
                return send_error(str(e))
        else:
            cherrypy.response.headers["Content-Type"] = "application/json"
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    def dpd_status(self, **kwargs):
        cherrypy.response.headers["Content-Type"] = "application/json"
        return send_ok(self.dpd.status())
