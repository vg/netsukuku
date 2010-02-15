# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2010 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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
# Tests for MapRoute.choose_between
#

##

import sys
sys.path.append('..')

import unittest

from ntk.core.route import MapRoute, RouteNode, Route, Rtt

def node_get(self, lvl, pos):
    ret = RouteNode(None, None, None, None)
    return ret
def is_free(self):
    return False
def best_route(self):
    class fakegw(object):
        def __init__(self):
            self.rem = Rtt(0)
    ret = Route(fakegw(), Rtt(123), None, None)
    return ret
def route_repr(self):
    return 'Route'

MapRoute.node_get = node_get
RouteNode.is_free = is_free
RouteNode.best_route = best_route
Route.__repr__ = route_repr

class TestChooseBetween(unittest.TestCase):

    def setUp(self):
        self._map = MapRoute(None, levels=4, gsize=256, me=[4, 3, 2, 1])
        self._set = []
        self._set.append([5, 3, 2, 3])
        self._set.append([5, 3, 2, 2])
        self._set.append([5, 3, 2, 21])
        self._set.append([5, 3, 2, 31])
        self._set.append([5, 3, 2, 41])

    def testChooseBetween(self):
        ret = self._map.choose_between(self._set)
        self.failUnless(ret in self._set)

    def testChooseMyself(self):
        ret = self._map.choose_between([[4, 3, 2, 1]])
        self.failUnless(ret == [4, 3, 2, 1])

if __name__ == '__main__':
    unittest.main()

