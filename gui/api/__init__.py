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

class API:
    def __init__(self, mod_rc):
        self.mod_rc = mod_rc

    @cherrypy.expose
    def index(self):
        return """This is the api area."""

    @cherrypy.expose
    def rc_parameters(self):
        cherrypy.response.headers["Content-Type"] = "application/json"
        return json.dumps(self.mod_rc.get_modules()).encode()

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
                return "{}".format(e)
            return json.dumps("ok").encode()
        else:
            cherrypy.response.headers["Content-Type"] = "application/json"
            cherrypy.response.status = 400
            return json.dumps("POST only").encode()
