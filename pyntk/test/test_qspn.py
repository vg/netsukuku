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
# Tests for ntk.core.qspn
#

import unittest

import sys
sys.path.append('..')

import stackless

from ntk.core.qspn import Etp
from ntk.core.radar import Neighbour, Neigh
from ntk.core.route import MapRoute, Rtt

IP = 16909060
MAX_NEIGHBOUR = 5
NETID = 19
NEIGH = Neigh(bestdev=('eth0', 42),
              devs={'eth0': 42},
              ip=IP,
              netid=NETID,
              idn=1)

class FakeRadar(object):

    def __init__(self):

        self.neigh = Neighbour()
        self.netid = -1

class TestEtp(unittest.TestCase):

    def setUp(self):
        radar = FakeRadar()
        maproute = MapRoute(levels=4, gsize=256, me=[8, 19, 82, 84])
        self.etp = Etp(radar, maproute)

    def test_collision_check(self):
        '''Collision check'''



if __name__ == '__main__':
    unittest.main()
