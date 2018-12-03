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
import json

class ConfigurationNotPresent:
    pass

class Configuration:
    def __init__(self, configfilename="ui-config.json"):
        self.config = None

        try:
            fd = open(configfilename, "r")
            self.config = json.load(fd)
        except json.JSONDecodeError:
            pass
        except OSError:
            pass

    def get_key(self, key):
        if self.config is None:
            raise ConfigurationNotPresent()
        else:
            return self.config[key]
