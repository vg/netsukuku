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
        self.levels = 3
        self.gsize  = 255
        self.dataclass = DataClass

        self.map = Map(self.levels, self.gsize, self.dataclass)

    def testLenListNode(self):
        '''Test node list len'''
        self.assertEqual(len(self.map.node), 3)

    def testNode_get(self):
        '''Test node_get'''
        self.failUnless(isinstance(self.map.node_get(1,3), DataClass))

    def testNode_add(self):
        '''Test adding a node'''
        self.assertEqual(self.map.node_nb[0], 0)
        self.map.node_add(lvl=0, id=0)
        self.assertEqual(self.map.node_nb[0], 1)

    def testNode_del(self):
        '''Test deleting a node'''
        self.testNode_add() # Adding a new node
        self.failUnless(isinstance(self.map.node[0][0], DataClass))

        self.map.node_del(lvl=0, id=0)
        self.assertEqual(self.map.node[0][0], None)

    def testFree_node_nb(self):
        '''Testing number of free nodes of levels'''
        self.assertEqual(self.map.free_nodes_nb(lvl=0), self.gsize)

    def testIs_in_level(self):
        ''''Test if node nip belongs to our gnode of level lvl'''
        self.assertEqual(self.map.is_in_level(self.map.me, 0), True)

    def testIP_to_NIP(self):
        '''Test conversion IP -> NIP (Netsukuku IP)'''
        self.assertEqual(self.map.ip_to_nip(127), [127, 0, 0])

    def testNIP_to_IP(self):
        '''Test conversion IP -> NIP (Netsukuku IP)'''
        self.assertEqual(self.map.nip_to_ip([127, 0, 0]), 127)

    def testNIP_cmp(self):
        '''Testing NIP comparing'''
        self.assertEqual(self.map.nip_cmp([127,0,1], [127,0,0]), 2)

if __name__ == '__main__':
    unittest.main()

