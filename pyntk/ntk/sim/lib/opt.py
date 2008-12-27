#!/usr/bin/env python
##
# This file is part of Netsukuku
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
#
# This source code is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This source code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# Please refer to the GNU Public License for more details.
#
# You should have received a copy of the GNU Public License along with
# this source code; if not, write to:
# Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
#
# A simple configuration file and command line options manager
#


class OptErr(Exception):pass

class Opt(object):
    def __init__(self, short_opts={}):
        self._short = short_opts

    def __getattr__(self, str):
        return None

    def subs_short_opts(self):
        for k, v in self._short.iteritems():
            if k in self.__dict__:
                self.__dict__[v] = self.__dict__[k]

    def load_file(self, path):
        execfile(path, self.__dict__, self.__dict__)
        self.subs_short_opts()

    def load_argv(self, argv):
        for s in argv[1:]:
            if '=' not in s:
                setattr(self, s, True)
            else:
                exec s in self.__dict__, self.__dict__
        self.subs_short_opts()

    def getdict(self, keys=[]):
        d = {}
        if keys == []:
            keys = self.__dict__.keys()
        for k in keys:
            if k not in self.__dict__:
                continue
            v = self.__dict__[k]
            if k[0] != '_' and k not in self._short:
                d[k]=v
        return d

    def __str__(self):
        return str(self.__dict__.keys())
