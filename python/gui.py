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

from multiprocessing import Process, Pipe
import os.path
import cherrypy
import configparser
import argparse
from jinja2 import Environment, FileSystemLoader
from gui.api import API
from lib import zmqrc

base_js = ["js/odr.js"]
base_css = ["css/odr.css"]

class Root:
    def __init__(self, dpd_port, end):
        self.mod_rc = zmqrc.ModRemoteControl("localhost")
        self.api = API(self.mod_rc, dpd_port)
        self.env = env

    @cherrypy.expose
    def index(self):
        raise cherrypy.HTTPRedirect('/home')

    @cherrypy.expose
    def about(self):
        tmpl = self.env.get_template("about.html")
        return tmpl.render(tab='about', js=base_js, is_login=False)

    @cherrypy.expose
    def home(self):
        tmpl = self.env.get_template("home.html")
        js = base_js + ["js/odr-home.js"]
        return tmpl.render(tab='home', js=js, css=base_css, is_login=False)

    @cherrypy.expose
    def rcvalues(self):
        tmpl = self.env.get_template("rcvalues.html")
        js = base_js + ["js/odr-rcvalues.js"]
        return tmpl.render(tab='rcvalues', js=js, is_login=False)

    @cherrypy.expose
    def modulator(self):
        tmpl = self.env.get_template("modulator.html")
        js = base_js + ["js/odr-modulator.js"]
        return tmpl.render(tab='modulator', js=js, is_login=False)

    @cherrypy.expose
    def predistortion(self):
        tmpl = self.env.get_template("predistortion.html")
        js = base_js + ["js/odr-predistortion.js"]
        return tmpl.render(tab='predistortion', js=js, is_login=False)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ODR-DabMod Web GUI')
    parser.add_argument('-c', '--config',
            default="gui-dpdce.ini",
            help='configuration filename')
    cli_args = parser.parse_args()

    allconfig = configparser.ConfigParser()
    allconfig.read(cli_args.config)
    config = allconfig['gui']
    dpd_port = allconfig['dpdce'].getint('control_port')
    plot_relative_dir = allconfig['dpdce']['plot_directory']

    daemon = False
    if daemon:
        cherrypy.process.plugins.Daemonizer(cherrypy.engine).subscribe()

    accesslog = os.path.realpath(os.path.join(config['logs_directory'], 'access.log'))
    errorlog = os.path.realpath(os.path.join(config['logs_directory'], 'error.log'))

    cherrypy.config.update({
            'engine.autoreload.on': True,
            'server.socket_host': config['host'],
            'server.socket_port': config.getint('port'),
            'request.show_tracebacks' : True,
            'tools.sessions.on': False,
            'tools.encode.on': True,
            'tools.encode.encoding': "utf-8",
            'log.access_file': accesslog,
            'log.error_file': errorlog,
            'log.screen': True,
            })

    staticdir = os.path.realpath(config['static_directory'])
    templatedir = os.path.realpath(config['templates_directory'])
    env = Environment(loader=FileSystemLoader(templatedir))

    cherrypy.tree.mount(
            Root(dpd_port, env), config={
                '/': { },
                '/dpd': {
                    'tools.staticdir.on': True,
                    'tools.staticdir.dir': os.path.realpath(plot_relative_dir)
                    },
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

