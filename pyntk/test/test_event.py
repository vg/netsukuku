# This file is part of Netsukuku
#
# Tests for ntk.lib.event

import sys
import unittest
sys.path.append('..')

from functools import partial
from ntk.lib.event import Event, EventError

DICT_EV = {}

def ev_listener(ev='', *msg):
    DICT_EV[ev] = msg

class TestEvent(unittest.TestCase):

    def setUp(self):
        self.events = Event(['A', 'B'])

    def testAddEvent(self):
        '''Test adding a new event'''
        self.events.add(['C'])
        self.failUnless(self.events.events == ['A', 'B', 'C'])

    def testEventListenFailure(self):
        '''Test listening of unregistered event'''

        self.assertRaises(EventError,
                          self.events.listen,
                          'D', partial(ev_listener, 'D'))

    def testEventSendFailure(self):
        '''Test sending of unregistered event'''
        self.assertRaises(EventError,
                          self.events.send,
                          'D', 'Message...')

    def testEventListen(self):
        '''Test event listening'''
        self.events.listen('A', partial(ev_listener, 'A'))
        self.events.send('A', (1,2,3,4))

        self.failUnlessEqual(DICT_EV['A'], (1,2,3,4))

if __name__ == '__main__':
    unittest.main()
