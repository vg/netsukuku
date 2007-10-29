#
# Tests for ntk.core.route
#

import sys
import unittest
sys.path.append('..')

from ntk.core.route import NullRem, DeadRem, Rtt, Bw, Avg

class TestRouteEfficiencyMeasure(unittest.TestCase):

    def setUp(self):
        rem_value = 1
        self.null_rem = NullRem(rem_value)
        self.dead_rem = DeadRem(rem_value)
        self.rtt = Rtt(rem_value)
        self.bw = Bw(rem_value, 1, 1)

        # TODO self.avg = Avg(...)

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
        newRtt = self.rtt + self.null_rem
        self.failUnless(isinstance(newRtt, Rtt))
        self.failUnlessEqual(newRtt.value,
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

if __name__ == '__main__':
    unittest.main()