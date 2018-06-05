#!/usr/bin/env python
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

import configuration
import os.path
import cherrypy
import argparse
from jinja2 import Environment, FileSystemLoader
from api import API
import zmqrc

env = Environment(loader=FileSystemLoader('templates'))

class Root:
    def __init__(self, config_file):
        self.config_file = config_file
        self.conf = configuration.Configuration(self.config_file)
        self.mod_rc = zmqrc.ModRemoteControl("localhost")
        self.api = API(self.mod_rc)

    @cherrypy.expose
    def index(self):
        raise cherrypy.HTTPRedirect('/home')

    @cherrypy.expose
    def about(self):
        tmpl = env.get_template("about.html")
        js = []
        return tmpl.render(tab='about', js=js, is_login=False)

    @cherrypy.expose
    def home(self):
        tmpl = env.get_template("home.html")
        js = []
        return tmpl.render(tab='home', js=js, is_login=False)

    @cherrypy.expose
    def rcvalues(self):
        tmpl = env.get_template("rcvalues.html")
        js = ["js/odr-rcvalues.js"]
        return tmpl.render(tab='rcvalues', js=js, is_login=False)

    @cherrypy.expose
    def modulator(self):
        tmpl = env.get_template("modulator.html")
        js = ["js/odr-modulator.js"]
        return tmpl.render(tab='modulator', js=js, is_login=False)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ODR-DabMod Web GUI')
    parser.add_argument('-c', '--config',
            default="ui-config.json",
            help='configuration filename')
    cli_args = parser.parse_args()

    config = configuration.Configuration(cli_args.config)
    if config.config is None:
        print("Configuration file is missing or is not readable - {}".format(cli_args.config))
        sys.exit(1)

    if config.config['global']['daemon']:
        cherrypy.process.plugins.Daemonizer(cherrypy.engine).subscribe()

    accesslog = os.path.realpath(os.path.join(config.config['global']['logs_directory'], 'access.log'))
    errorlog = os.path.realpath(os.path.join(config.config['global']['logs_directory'], 'error.log'))

    cherrypy.config.update({
            'server.socket_host': config.config['global']['host'],
            'server.socket_port': int(config.config['global']['port']),
            'request.show_tracebacks' : True,
            'environment': 'production',
            'tools.sessions.on': False,
            'tools.encode.on': True,
            'tools.encode.encoding': "utf-8",
            'log.access_file': accesslog,
            'log.error_file': errorlog,
            'log.screen': True,
            })

    staticdir = os.path.realpath(config.config['global']['static_directory'])

    cherrypy.tree.mount(
            Root(cli_args.config), config={
                '/': { },
                '/css': {
                    'tools.staticdir.on': True,
                    'tools.staticdir.dir': os.path.join(staticdir, u"css/")
                    },
                '/js': {
                    'tools.staticdir.on': True,
                    'tools.staticdir.dir': os.path.join(staticdir, u"js/")
                    },
                '/fonts': {
                    'tools.staticdir.on': True,
                    'tools.staticdir.dir': os.path.join(staticdir, u"fonts/")
                    },
                '/favicon.ico': {
                    'tools.staticfile.on': True,
                    'tools.staticfile.filename': os.path.join(staticdir, u"fonts/favicon.ico")
                    },
                }
            )

    cherrypy.engine.start()
    cherrypy.engine.block()

