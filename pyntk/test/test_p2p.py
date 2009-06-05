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
# Tests for ntk.core.p2p
#

import unittest

import sys
sys.path.append('..')

import stackless

from ntk.core.p2p import MapP2P, ParticipantNode, P2P, P2PAll
from ntk.core.radar import Neighbour, Neigh
from ntk.core.route import MapRoute, Rtt

from utils import BaseObserver

class MapP2PObserver(BaseObserver):

    EVENTS = ['ME_CHANGED', 'NODE_DELETED']

    def me_changed(self, old_me, new_me):
        self.me_changed_event = (old_me, new_me)

    def node_deleted(self, lvl, id):
        self.node_deleted_event = (lvl, id)

class FakeRadar(object):

    def __init__(self):

        self.neigh = Neighbour()

class TestMapP2P(unittest.TestCase):

    def setUp(self):
        levels = 4
        gsize = 256

        self.map = MapP2P(levels, gsize, me=[8, 19, 82, 85], pid=1234)
        self.observer = MapP2PObserver(who=self.map)

    def testParticipant(self):
        '''Test if node participate to service'''
        for l in range(self.map.levels):
            self.failUnlessEqual(self.map.node[l][self.map.me[l]], None)

        self.map.participate()

        for l in range(self.map.levels):
            self.failUnless(isinstance(self.map.node[l][self.map.me[l]],
                                       ParticipantNode))
            self.failUnlessEqual(self.map.node[l][self.map.me[l]].participant,
                                 True)

    def testMeChanged(self):
        '''Test my nip change'''
        old_me = self.map.me
        self.map.me_changed([19, 8, 85, 82])

        stackless.run()

        old_me_observed, new_me_observed = self.observer.me_changed_event

        self.failUnlessEqual(old_me, old_me_observed)
        self.failUnlessEqual(self.map.me, new_me_observed)

    def testNodeDel(self):
        '''Test deleting a node'''

        self.map.node_add(lvl=2, id=8)
        self.failUnless(isinstance(self.map.node_get(2, 8), ParticipantNode))

        self.map.node_del(lvl=2, id=8)

        stackless.run()
        self.failUnlessEqual(self.map.node[2][8], None)

        lvl_observed, id_observed = self.observer.node_deleted_event
        self.failUnlessEqual(lvl_observed, 2)
        self.failUnlessEqual(id_observed, 8)

class TestP2P(unittest.TestCase):

    def setUp(self):
        radar = FakeRadar()
        maproute = MapRoute(levels=4, gsize=256, me=[8, 19, 82, 85])

        self.p2p = P2P(radar, maproute, pid=819)

    def testParticipantAdd(self):
        '''Adding a participant node to the P2P service'''
        for l in range(self.p2p.mapp2p.levels):
            self.failUnlessEqual(self.p2p.mapp2p.free_nodes_nb(l),
                                 self.p2p.mapp2p.gsize)

        self.p2p.participant_add([19, 19, 82, 85])

        for l in range(self.p2p.mapp2p.levels):
            self.failUnlessEqual(self.p2p.mapp2p.free_nodes_nb(l),
                                 self.p2p.mapp2p.gsize - 1)

    def test_h(self):
        '''Test h function: KEY->IP'''
        self.failUnlessEqual(self.p2p.h('key'), 'key')

    def testH(self):
        '''Test H function: IP->IP*'''
        self.failUnlessEqual(self.p2p.H([1, 2, 3, 4]), None)

        self.p2p.participant_add([2, 19, 82, 85])

        self.failUnlessEqual(self.p2p.H([1, 2, 3, 4]), [2, 19, 82, 85])

    def testParticipate(self):
        '''Self participation to the P2P service'''
        for l in range(self.p2p.mapp2p.levels):
            self.failUnlessEqual(self.p2p.mapp2p.free_nodes_nb(l),
                                 self.p2p.mapp2p.gsize)

        self.p2p.participate()

        for l in range(self.p2p.mapp2p.levels):
            self.failUnlessEqual(self.p2p.mapp2p.free_nodes_nb(l),
                                 self.p2p.mapp2p.gsize)

    def testNeighGet(self):
        '''Getting the neighbour reach the hash node'''
        self.failUnlessEqual(self.p2p.neigh_get([2, 19, 82, 85]), None)

        n = [2, 19, 82, 85]
        self.p2p.participant_add(n)

        IP = 84215045
        NETID = 123456
        NEIGH = Neigh(bestdev=('eth0', 1),
                      devs={'eth0': 1},
                      ip=IP,
                      netid=NETID,
                      idn=5)
        self.p2p.neigh.netid_table[IP] = NETID
        self.p2p.neigh.translation_table[IP] = 5
        self.p2p.neigh.store({IP:NEIGH})

        for lvl, id, in enumerate(n):
            self.p2p.maproute.route_add(lvl, id, 5, Rtt(1))

        self.failUnlessEqual(self.p2p.neigh_get(n).values(), NEIGH.values())


class TestP2PAll(unittest.TestCase):

    def setUp(self):
        radar = FakeRadar()
        maproute = MapRoute(levels=4, gsize=256, me=[8, 19, 82, 85])
        self.p2pall = P2PAll(radar, maproute)

    def testPidAdd(self):
        '''Add a new P2P id'''
        self.failUnlessEqual(self.p2pall.service, {})
        self.p2pall.pid_add(1234)
        self.failUnless(self.p2pall.service.has_key(1234))

    def testPidGet(self):
        '''Get a P2P id'''
        self.p2pall.pid_get(1234)
        self.failUnless(self.p2pall.service.has_key(1234))

    def testPidDel(self):
        '''Deleting a P2P id'''
        self.p2pall.pid_add(1234)
        self.failUnless(self.p2pall.service.has_key(1234))
        self.p2pall.pid_del(1234)
        self.assertFalse(self.p2pall.service.has_key(1234))
        self.failUnlessEqual(self.p2pall.service, {})

    #def testPidGetAll(self):
        #'''Get all P2P id'''
        #self.failUnlessEqual(self.p2pall.pid_getall(), [])
        #self.p2pall.pid_add(1234)
        #self.p2pall.pid_add(12345)
        ## TODO: Add a test for ntk.map.Map.map_data_pack

    #def testP2PHook(self):
        #'''Hooking Fase'''
        ## TODO: use mock objects to simulate network calls

if __name__ == '__main__':
    unittest.main()
