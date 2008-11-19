# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
#
# Tests for ntk.network.Route


import os
import sys
import unittest
sys.path.append('..')

from ntk.network import Route

if sys.platform == 'linux2':
    from ntk.network.linux.adapt import IPROUTECommandError
    if os.geteuid() != 0:
        sys.exit('You must be root to run this test')

class TestRoute(unittest.TestCase):

    def setUp(self):
        self.ip = '172.26.0.0'
        self.cidr = 16
        self.gateway = '172.26.26.1'
        # Flush the `ntk' table
        # Route.flush()

    def testAddRouteError(self):
        ''' Test add a new route with incomplete parameters '''
        self.assertRaises(IPROUTECommandError, Route.add, self.ip, self.cidr)

    def testAddRoute(self):
        ''' Test add a new route '''
        Route.add(self.ip, self.cidr, dev='eth0')

    def testDeleteRoute(self):
        ''' Test remove a route '''
        Route.delete(self.ip, self.cidr, dev='eth0')

    def testFlushRoutes(self):
        ''' Test flush routes from `ntk' table '''
        self.testAddRoute()
        Route.flush()

    def testFlushCache(self):
        ''' Test flush cache '''
        Route.flush_cache()

    def testEnableIPForwarding(self):
        ''' Test enable ip forwarding '''
        Route.ip_forward()

if __name__ == '__main__':
    unittest.main()
