##
# This file is part of Netsukuku
# (c) Copyright 2009 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
# Tests for ntk.core.coord
#


import itertools
import time
import unittest

import sys
sys.path.append('..')

import stackless

from ntk.core.coord import Coord, MapCache
from ntk.core.p2p import P2PAll
from ntk.core.radar import Neighbour
from ntk.core.route import MapRoute

class TestMapCache(unittest.TestCase):

    def setUp(self):
        maproute = MapRoute(levels=4, gsize=256, me=[8, 19, 82, 84])
        self.map = MapCache(maproute)

    def test_node_add(self):
        '''Test adding a new node'''
        self.failUnlessEqual(self.map.node_nb[2], 0)
        self.map.node_add(lvl=2, id=8)
        self.failUnlessEqual(self.map.node_nb[2], 1)

    def test_node_del(self):
        '''Test adding a new node'''
        self.failUnlessEqual(self.map.node_nb[2], 0)
        self.map.node_add(lvl=2, id=8)
        self.failUnlessEqual(self.map.node_nb[2], 1)

    def test_tmp_deleted_add(self):
        '''Test adding an entry in tmp_deleted cache'''
        self.test_node_add()
        self.map.tmp_deleted_add(lvl=2, id=8)
        self.failUnless(self.map.tmp_deleted != {})

    def test_tmp_deleted_del(self):
        '''Test deleting an entry from tmp_deleted cache'''
        self.test_tmp_deleted_add()
        self.map.tmp_deleted_del(lvl=2, id=8)
        self.failUnlessEqual(self.map.tmp_deleted, {})

    #def test_tmp_deleted_purge(self):
        #'''Testing purge tmp_deleted cache'''

        #for id in range(20):
            #self.map.node_add(lvl=2, id=id)

        #for id in range(10):
            #self.map.tmp_deleted_add(lvl=2, id=id)

        #self.failUnlessEqual(len(self.map.tmp_deleted), 10)

        #time.sleep(2)
        #self.map.tmp_deleted_purge(timeout=1)
        #stackless.run()
        #self.failUnlessEqual(self.map.tmp_deleted, {})

class FakeRadar(object):

    def __init__(self):

        self.neigh = Neighbour()

class TestCoord(unittest.TestCase):

    def setUp(self):
        radar = FakeRadar()
        maproute = MapRoute(levels=4, gsize=256, me=[8, 19, 82, 84])
        p2pall = P2PAll(radar, maproute)
        self.coord = Coord(radar, maproute, p2pall)

    def test_h(self):
        '''Test h function: KEY->IP'''
        me = self.coord.maproute.me
        for i in range(self.coord.maproute.levels):
            self.failUnlessEqual(self.coord.h((i, self.coord.maproute.me)),
                                 list(itertools.chain([0 for _ in range(i)],
                                                      me[i:])))

    def test_coornodes_set(self):
        '''Set up of the coordinator nodes of each level'''
        self.failUnlessEqual(self.coord.coordnode,
                             [None] * self.coord.maproute.levels)

        self.coord.participant_add([2, 19, 82, 84])

        self.coord.coornodes_set()
        self.failUnlessEqual(self.coord.coordnode,
                             [[2, 19, 82, 84]] * self.coord.maproute.levels)

    def test_new_participant_joined(self):
        '''New participant joined'''
        self.failUnlessEqual(self.coord.coordnode,
                             [None] * self.coord.maproute.levels)
        self.coord.participant_add([2, 19, 82, 84])
        self.coord.new_participant_joined(0, 2)
        stackless.run()
        self.failUnlessEqual(self.coord.coordnode,
                             [None, [2, 19, 82, 84], None, None])

    #def test_going_in(self):
        #'''A node become a member of our gnode'''

    #def test_going_out(self):
        #'''A node wants to leave our gnode'''

if __name__ == '__main__':
    unittest.main()
