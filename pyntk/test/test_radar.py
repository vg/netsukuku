# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
#
# Tests for ntk.core.radar

import sys
import unittest
sys.path.append('..')

from ntk.core.radar import Neigh, Neighbour

class TestNeighbour(unittest.TestCase):

    def setUp(self):
        self.neighbour = Neighbour()

    def testEmptyNeighList(self):
        ''' Empty neigh list '''
        self.failUnless(self.neighbour.neigh_list() == [])

    

if __name__ == '__main__':
    unittest.main()
