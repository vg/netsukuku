# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
#
# Tests for ntk.core.route

import sys
import unittest
sys.path.append('..')

from ntk.core.route import NullRem, DeadRem, Rtt, Bw, Avg, RouteGw, RouteNode

class TestRouteEfficiencyMeasure(unittest.TestCase):

    def setUp(self):
        rem_value = 1
        self.null_rem = NullRem(rem_value)
        self.dead_rem = DeadRem(rem_value)
        self.rtt = Rtt(rem_value)
        self.bw = Bw(rem_value, 1, 1)

    def testAdd2NullRem(self):
        '''Test adding 2 NullRem'''
        self.failUnless(NullRem(1) + self.null_rem is self.null_rem)

    def testAddNullRemDeadRem(self):
        '''Test adding a NullRem with a DeadRem'''
        self.failUnless(self.null_rem + self.dead_rem is self.dead_rem)

    def testAddDeadRemNullRem(self):
        '''Test adding a DeadRem with a NullRem'''
        self.failUnless(self.dead_rem + self.null_rem is self.dead_rem)

    def testAddRttNullRem(self):
        '''Test adding a Rtt with a NullRem'''
        new_rtt = self.rtt + self.null_rem
        self.failUnless(isinstance(new_rtt, Rtt))
        self.failUnlessEqual(new_rtt.value,
                             self.rtt.value + self.null_rem.value)

    def testAddRttDeadRem(self):
        '''Test adding a Rtt with a DeadRem'''
        self.failUnless(self.rtt + self.dead_rem is self.dead_rem)

    def testCompareRtt(self):
        '''Comparing 2 Rtt'''

        # self.rtt is better than Rtt(5)
        self.failUnless(self.rtt > Rtt(5))
        # Rtt(5) is worse than self.rtt
        self.failUnless(Rtt(5) < self.rtt)
        # self.rtt and Rtt(1) are the same
        self.failUnless(Rtt(1) == self.rtt)

    def testAddBwNullRem(self):
        '''Test adding a Bw with a NullRem'''
        new_bw = self.bw + self.null_rem
        self.failUnless(isinstance(new_bw, Bw))
        self.failUnlessEqual(new_bw.value,
                             min(self.bw.value, self.null_rem.value))

    def testAddBwDeadRem(self):
        '''Test adding a Rtt with a DeadRem'''
        self.failUnless(self.bw + self.dead_rem is self.dead_rem)

    def testCompareBw(self):
        '''Comparing 2 Bw'''

        # self.bw is worse than Bw(5)
        self.failUnless(self.bw < Bw(5, 1, 1))
        # Bw(5) is better than self.bw
        self.failUnless(Bw(5, 1, 1) > self.bw)
        # self.bw and Bw(1) are the same
        self.failUnless(Bw(1, 1, 1) == self.bw)

    def testAvgInit(self):
        '''Test Avg initialization'''

class TestRouteGw(unittest.TestCase):

    def setUp(self):
        self.route_gw = RouteGw(object(), Rtt(1))

    def testCompareRouteGw(self):
        '''Comparing 2 RouteGw'''
        self.failUnless(self.route_gw > RouteGw(object(), Rtt(10)))

    def testRemModify(self):
        '''Test modifying rem'''
        self.failUnless(self.route_gw.rem_modify(Rtt(1)) == Rtt(1))

        self.failUnless(self.route_gw.rem_modify(Rtt(10)) == Rtt(1))
        self.failUnless(self.route_gw.rem == Rtt(10))

if __name__ == '__main__':
    unittest.main()