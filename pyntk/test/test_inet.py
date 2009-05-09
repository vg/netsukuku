##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
# Tests for ntk.network.inet
#

import unittest

import sys
sys.path.append('..')

from ntk.config import settings
from ntk.network import inet

class TestInet(unittest.TestCase):

    def setUp(self):
        self.ps = '1.2.3.4'

    def testLevelToBits(self):
        ''' Test level to bits conversion '''

        levels = range(settings.LEVELS)
        bits = range(inet.ipbit[settings.IP_VERSION],
                     0,
                     -(settings.BITS_PER_LEVEL))

        for l in levels:
            self.assertEqual(inet.lvl_to_bits(l), bits[l])

    def testStrToPip(self):
        ''' Test conversion str --> pip '''
        self.assertEqual(inet.str_to_pip(self.ps), '\x01\x02\x03\x04')

    def testPipToIP(self):
        ''' Test conversion pip --> ip '''
        pip = inet.str_to_pip(self.ps)
        self.assertEqual(inet.pip_to_ip(pip), 16909060)

    def testIPToPip(self):
        ''' Test conversion ip --> pip '''
        ip = inet.pip_to_ip(inet.str_to_pip(self.ps))
        self.assertEqual(inet.ip_to_pip(ip), '\x01\x02\x03\x04')

    def testIPToStr(self):
        ''' Test conversion ip --> str '''
        ip = inet.pip_to_ip(inet.str_to_pip(self.ps))
        self.assertEqual(inet.ip_to_str(ip), self.ps)

    def testStrToIP(self):
        ''' Test conversion str --> ip '''
        self.assertEqual(inet.str_to_ip(self.ps), 16909060)

if __name__ == '__main__':
    unittest.main()
