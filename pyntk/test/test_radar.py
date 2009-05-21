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
# Tests for ntk.core.radar
#


import copy
import sys
import unittest

from random import randint
sys.path.append('..')

from ntk.core.radar import Neigh, Neighbour
from ntk.core.route import DeadRem

IP = 16909060
MAX_NEIGHBOUR = 5
NETID = randint(0, 2**32-1)
NEIGH = Neigh(bestdev=('eth0', 42),
              devs={'eth0': 42},
              ip=IP,
              netid=NETID,
              idn=1)

class Observer(object):

    def __init__(self, neighbour):
        self.delete_events = []
        self.new_events = []
        self.rem_changed_events = []

        neighbour.events.listen('NEIGH_DELETED', self.neigh_deleted)
        neighbour.events.listen('NEIGH_NEW', self.neigh_new)
        neighbour.events.listen('NEIGH_REM_CHGED', self.neigh_rem_changed)

    def neigh_new(self, neighbour):
        self.new_events.append(neighbour)

    def neigh_deleted(self, neighbour):
        self.delete_events.append(neighbour)

    def neigh_rem_changed(self, neighbour, rem):
        self.rem_changed_events.append((neighbour, rem))

class TestNeighbour(unittest.TestCase):

    def setUp(self):
        self.neighbour = Neighbour(max_neigh=MAX_NEIGHBOUR)
        self.observer = Observer(self.neighbour)

    def testEmptyNeighbourList(self):
        '''Empty neighbour list'''
        self.failUnlessEqual(self.neighbour.neigh_list(), [])
        self.failUnlessEqual(self.neighbour.ip_to_neigh(IP), None)

    def testAddNeighbour(self):
        '''Add a new neighbour'''
        ip_table = {IP: NEIGH}
        self.neighbour.netid_table[IP] = NETID
        self.neighbour.store(ip_table)

        n = self.neighbour.neigh_list()[0]
        self.failUnlessEqual(n, NEIGH)

        self.failUnlessEqual(self.neighbour.ip_to_neigh(IP).values(),
                             NEIGH.values())

        neighbour_observed = self.observer.new_events.pop()
        self.failUnlessEqual(NEIGH.values(), neighbour_observed.values())

    def testDeleteNeighbour(self):
        '''Delete a neighbour'''
        self.testAddNeighbour()

        self.neighbour.delete(IP)
        self.failUnlessEqual(self.neighbour.neigh_list(), [])

        neighbour_observed = self.observer.delete_events.pop()
        deleted_neighbour = copy.copy(NEIGH)
        deleted_neighbour.devs = deleted_neighbour.bestdev = None
        deleted_neighbour.rem = DeadRem()
        self.failUnlessEqual(deleted_neighbour.values(),
                             neighbour_observed.values())

    def testChangeNeighbourRem(self):
        '''Change neighbour REM'''
        self.testAddNeighbour()
        changed_neighbour = Neigh(bestdev=('eth0', 8),
                                  devs={'eth0': 8},
                                  ip=IP,
                                  idn=1)
        ip_table = {IP:changed_neighbour}
        self.neighbour.store(ip_table)

        n = self.neighbour.neigh_list()[0]
        self.failUnlessEqual(n.rem.value, 8)

        (neighbour_observed,
         rem_observed) = self.observer.rem_changed_events.pop()
        self.failUnlessEqual(NEIGH.values(),
                             neighbour_observed.values())
        self.failUnlessEqual(rem_observed.value, 8)

    def testNeighbourIPChange(self):
        '''Neighbour IP change'''
        self.testAddNeighbour()
        new_ip = 84281096
        new_neighbour = copy.copy(NEIGH)
        new_neighbour.ip = new_ip

        self.neighbour.ip_change(IP, new_ip)

        self.failUnlessEqual(self.neighbour.ip_to_neigh(IP), None)
        self.failUnlessEqual(self.neighbour.ip_to_neigh(new_ip),
                             new_neighbour)

        neighbour_observed = self.observer.new_events.pop()
        self.failUnlessEqual(new_neighbour.values(),
                             neighbour_observed.values())

    def testFindHoleInTranslationTable(self):
        '''Find hole in traslation table'''
        self.failUnlessEqual(self.neighbour._find_hole_in_tt(), 1)
        self.testAddNeighbour()
        self.failUnlessEqual(self.neighbour._find_hole_in_tt(), 2)

    def testTruncate(self):
        '''Truncate ip_table'''
        neighbours = [Neigh(bestdev=('eth0', randint(1,100)),
                            devs=None,
                            ip=randint(0, 2**32 -1),
                            netid=NETID,
                            idn=1) for _ in range(10)]

        ip_table = dict([(n.ip, n) for n in neighbours])

        f = lambda neighbour: neighbour.bestdev[1] # Get Rem value from
                                                   # the neighbour to sort
                                                   # properly neighbours list
        neighbours.sort(key=f)
        best_neighbours = neighbours[:MAX_NEIGHBOUR]

        trunc_ip_table, truncated = self.neighbour._truncate(ip_table)

        self.failUnlessEqual(sorted(trunc_ip_table.values(), key=f),
                             best_neighbours)

if __name__ == '__main__':
    unittest.main()
