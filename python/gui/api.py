# -*- coding: utf-8 -*-
#
#   Copyright (C) 2019
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

from lib import yamlrpc

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

class API:
    def __init__(self, mod_rc, dpd_port):
        self.mod_rc = mod_rc
        self.dpd_port = dpd_port
        self.dpd_rpc = yamlrpc.Socket(bind_port=0)

    @cherrypy.expose
    def index(self):
        return """This is the api area."""

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def rc_parameters(self):
        try:
            return send_ok(self.mod_rc.get_modules())
        except IOError as e:
            return send_error(str(e))

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def parameter(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cl = cherrypy.request.headers['Content-Length']
            rawbody = cherrypy.request.body.read(int(cl))
            params = json.loads(rawbody.decode())
            try:
                self.mod_rc.set_param_value(params['controllable'], params['param'], params['value'])
            except IOError as e:
                cherrypy.response.status = 503
                return send_error(str(e))
            except ValueError as e:
                cherrypy.response.status = 503
                return send_error(str(e))
            return send_ok()
        else:
            if all(p in kwargs for p in ('controllable', 'param')):
                try:
                    return send_ok(self.mod_rc.get_param_value(kwargs['controllable'], kwargs['param']))
                except IOError as e:
                    cherrypy.response.status = 503
                    return send_error(str(e))
                except ValueError as e:
                    cherrypy.response.status = 503
                    return send_error(str(e))
            else:
                cherrypy.response.status = 400
                return send_error("missing 'controllable' or 'param' GET parameters")

    def _wrap_dpd(self, method, data=None):
        try:
            reply = self.dpd_rpc.call_rpc_method(self.dpd_port, method, data)
            return send_ok(reply)
        except ValueError as e:
            cherrypy.response.status = 503
            return send_error("DPDCE remote procedure call error: {}".format(e))
        except TimeoutError as e:
            cherrypy.response.status = 503
            return send_error("DPDCE remote procedure call timed out")
        cherrypy.response.status = 500
        return send_error("Unknown DPDCE remote procedure error error")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_trigger_run(self, **kwargs):
        if cherrypy.request.method == 'POST':
            return self._wrap_dpd("trigger_run")
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_adapt(self, **kwargs):
        if cherrypy.request.method == 'POST':
            return self._wrap_dpd("adapt")
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_reset(self, **kwargs):
        if cherrypy.request.method == 'POST':
            return self._wrap_dpd("reset")
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_restore_dump(self, **kwargs):
        if cherrypy.request.method == 'POST':
            cl = cherrypy.request.headers['Content-Length']
            rawbody = cherrypy.request.body.read(int(cl))
            params = json.loads(rawbody.decode())
            if 'dump_id' in params:
                data = {'dump_id': params['dump_id']}
                return self._wrap_dpd("restore_dump", data)
            else:
                cherrypy.response.status = 400
                return send_error("Missing dump_id")
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_results(self, **kwargs):
        return self._wrap_dpd("get_results")

    @cherrypy.expose
    @cherrypy.tools.json_out()
    def dpd_calibrate(self, **kwargs):
        if cherrypy.request.method == 'POST':
            return self._wrap_dpd("calibrate")
        else:
            cherrypy.response.status = 400
            return send_error("POST only")

