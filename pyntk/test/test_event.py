# This file is part of Netsukuku
#
# Tests for ntk.lib.event

import sys
import unittest
sys.path.append('..')

from functools import partial
from ntk.lib.event import Event, EventError, apply_wakeup_on_event
from ntk.lib.micro import micro, microfunc, allmicro_run, Channel
import ntk.wrap.xtime as xtime

DICT_EV = {}

def ev_listener(ev='', *msg):
    DICT_EV[ev] = msg

@microfunc(True)
def sending_signals(events, ret):
    xtime.swait(50)
    ret.append('A1')
    events.send('A', ('1',))
    xtime.swait(50)
    ret.append('B1')
    events.send('B', ('1',))
    xtime.swait(50)
    ret.append('A2')
    events.send('A', ('2',))
    xtime.swait(50)
    ret.append('B2')
    events.send('B', ('2',))
    xtime.swait(50)
    ret.append('A3')
    events.send('A', ('3',))

_events = Event(['A', 'B'])

def _testWakeupManySignalWaited(ret, event_wait=None):
    sending_signals(_events, ret)
    (num,) = event_wait[(_events, 'A')]()
    ret.append('after A1')
    ret.append('num=' + num)
    (num,) = event_wait[(_events, 'A')]()
    ret.append('after A2')
    ret.append('num=' + num)
    (num,) = event_wait[(_events, 'A')]()
    ret.append('after A3')
    ret.append('num=' + num)
    xtime.swait(100)

def _testWakeupDifferentSignalWaited(ret, event_wait=None):
    sending_signals(_events, ret)
    (num,) = event_wait[(_events, 'B')]()
    ret.append('after B1')
    ret.append('num=' + num)
    (num,) = event_wait[(_events, 'A')]()
    ret.append('after A2')
    ret.append('num=' + num)
    (num,) = event_wait[(_events, 'B')]()
    ret.append('after B2')
    ret.append('num=' + num)
    xtime.swait(100)

_testWakeupManySignalWaited = apply_wakeup_on_event(_testWakeupManySignalWaited, 
                                          events=[(_events, 'A')])
_testWakeupDifferentSignalWaited = apply_wakeup_on_event(_testWakeupDifferentSignalWaited, 
                                          events=[(_events, 'A'),
                                                  (_events, 'B')])

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
    
    def testWakeup1ManySignalWaited(self):
        '''Test waiting same signal many times'''
        ret = []
        _testWakeupManySignalWaited(ret)
        self.failUnlessEqual(ret, ['A1', 'after A1', 'num=1', 'B1', 'A2', 'after A2', 'num=2', 'B2', 'A3', 'after A3', 'num=3'])

    def testWakeup2DifferentSignalWaited(self):
        '''Test waiting different signals in correct order'''
        ret = []
        _testWakeupDifferentSignalWaited(ret)
        self.failUnlessEqual(ret, ['A1', 'B1', 'after B1', 'num=1', 'A2', 'after A2', 'num=2', 'B2', 'after B2', 'num=2', 'A3'])

if __name__ == '__main__':
    unittest.main()
