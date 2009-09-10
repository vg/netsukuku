##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
# Tests for ntk.core.map
#

import unittest

import sys
sys.path.append('..')

from ntk.core.map import Map, DataClass

class TestMap(unittest.TestCase):

    def setUp(self):
        self.levels = 4
        self.gsize  = 256
        self.dataclass = DataClass

        self.map = Map(self.levels, self.gsize, self.dataclass)

    def test_len_map_node(self):
        '''Map node len'''
        self.assertEqual(len(self.map.node), 4)

    def test_node_get(self):
        '''Get a new node'''
        self.failUnless(isinstance(self.map.node_get(1,3), DataClass))

    def test_node_add(self):
        '''Add a node'''
        self.assertEqual(self.map.node_nb[0], 0)
        self.map.node_add(lvl=0, id=0)
        self.assertEqual(self.map.node_nb[0], 1)

    def test_node_del(self):
        '''Delete a node'''
        self.test_node_add() # Add a new node
        self.failUnless(isinstance(self.map.node[0][0], DataClass))

        self.map.node_del(lvl=0, id=0)
        self.assertEqual(self.map.node[0][0], None)

    def test_free_node_nb(self):
        '''Number of free nodes of levels'''
        self.assertEqual(self.map.free_nodes_nb(lvl=0), self.gsize)
        self.test_node_add()
        self.assertEqual(self.map.free_nodes_nb(lvl=0), self.gsize - 1)

    def test_free_nodes_list(self):
        '''Free nodes of specified level'''
        self.assertEqual(self.map.free_nodes_list(lvl=0), range(self.gsize))

    def test_is_in_level(self):
        '''Node nip belongs to our gnode of level'''
        self.assertEqual(self.map.is_in_level(self.map.me, 0), True)

    def test_ip_to_nip(self):
        '''Conversion IP -> NIP (Netsukuku IP)'''
        self.assertEqual(self.map.ip_to_nip(127), [127, 0, 0, 0])

    def test_nip_to_ip(self):
        '''Conversion IP -> NIP (Netsukuku IP)'''
        self.assertEqual(self.map.nip_to_ip([127, 0, 0, 0]), 127)

    def test_nip_cmp(self):
        '''Comparing two NIP'''
        self.assertEqual(self.map.nip_cmp([127, 0, 0, 1], [127, 0, 0, 0]), 3)

if __name__ == '__main__':
    unittest.main()
