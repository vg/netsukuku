##
# This file is part of Netsukuku
# (c) Copyright 2010 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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

import os
import sys
import unittest
import time

sys.path.append('..')

log_on_console = True
log_container = []
def log_func(args):
    global log_on_console, log_count
    if log_on_console:
        print args
    log_container.append(args)

class fakelogger(object):
    def log(self, *args): log_func(args[1])
    def debug(self, *args): log_func(args)
    def error(self, *args): log_func(args)
    def info(self, *args): log_func(args)
    def warning(self, *args): log_func(args)
class fakeexception(Exception): pass
def faketrace(*args): log_func(args)

import ntk.lib.log
ntk.lib.log.logger = fakelogger()
ntk.lib.log.logger.ULTRADEBUG=1
ntk.lib.log.log_exception_stacktrace = faketrace
ntk.lib.log.ExpectableException = fakeexception

from ntk.lib.micro import allmicro_run, micro_block, microfunc, micro
from ntk.lib.micro import micro_kill, micro_current, micro_runnables
from ntk.lib.micro import time_swait, time_while_condition, Channel
from ntk.lib.micro import micro_reschedule_prio, micro_reschedule_me_asap

class TestMicro(unittest.TestCase):

    #TO TEST:
    #ch1 = Channel()
    #ch2 = Channel(True) # prefer_sender
    #ch3 = Channel(micro_send=True) # used by microsock

    def test010_send_prefer_sender(self):
        self.ret = []
        ch = Channel(True) # prefer_sender
        self.mf1 = micro(self.mf1_test010, (ch,))
        self.mf2 = micro(self.mf2_test010, (ch,))
        self.mf3 = micro(self.mf3_test010, (ch,))
        self.mf4 = micro(self.mf4_test010, (ch,))
        self.mf5 = micro(self.mf5_test010, (ch,))
        allmicro_run()
        self.failUnlessEqual(self.ret, [1, 2, 3, 4, 5, 6, 7, 8, 9])

    def mf1_test010(self, ch):
        self.failUnless(self.ret == [], 'mf1 not started first')
        self.ret.append(1)
        ch.send(0)
        self.failUnless(self.ret == [1, 2, 3], 'ch.recv in mf3 did not pass the schedule to sender mf1')
        self.ret.append(4)

    def mf2_test010(self, ch):
        self.failUnless(self.ret == [1], 'ch.send in mf1 did not pass the schedule to mf2')
        self.failUnless(self.mf1.blocked, 'ch.send in mf1 did not block mf1')
        self.failUnless(self.mf1.alive, 'ch.send in mf1, mf1 seems dead')
        self.ret.append(2)
        micro_block()
        self.failUnless(self.ret == [1, 2, 3, 4, 5], 'ch.recv in mf4 did not pass the schedule to mf2')
        self.failUnless(not self.mf1.alive, 'mf1 should be dead')
        self.failUnless(self.mf4.alive, 'mf4 should be alive')
        self.failUnless(self.mf4.blocked, 'mf4 should be blocked')
        self.ret.append(6)
        ch.send(1)
        self.failUnless(self.ret == [1, 2, 3, 4, 5, 6], 'ch.send in mf2 should not pass the schedule')
        self.failUnless(not self.mf1.alive, 'mf1 should be dead')
        self.ret.append(7)

    def mf3_test010(self, ch):
        self.failUnless(self.ret == [1, 2], 'micro_block in mf2 did not pass the schedule to mf3')
        self.failUnless(self.mf1.blocked, 'ch.send in mf1 did not block mf1')
        self.failUnless(self.mf1.alive, 'ch.send in mf1, mf1 seems dead')
        self.ret.append(3)
        secret = ch.recv()
        self.failUnless(secret == 0, 'ch.recv in mf3 did not return 0 (first message sent from mf1)')
        self.failUnless(self.ret == [1, 2, 3, 4, 5, 6, 7], 'mf3 not at the right moment')
        self.ret.append(8)

    def mf4_test010(self, ch):
        self.failUnless(self.ret == [1, 2, 3, 4], 'mf4 not in the right moment')
        self.failUnless(not self.mf1.alive, 'mf1 has finished, but mf1 still alive')
        self.ret.append(5)
        secret = ch.recv()
        self.failUnless(secret == 1, 'ch.recv in mf4 did not return 1 (second message sent from mf3)')
        self.failUnless(self.ret == [1, 2, 3, 4, 5, 6, 7, 8], 'mf4 not at the right moment')
        self.ret.append(9)

    def mf5_test010(self, ch):
        micro_block()
        micro_block()
        micro_block()
        micro_block()
        micro_block()
        micro_block()

    def test020_send_recv_timeout(self):
        self.ret = []
        ch = Channel() # no prefer_sender, no is_micro
        self.mf1 = micro(self.mf1_test020, (ch,))
        self.mf2 = micro(self.mf2_test020, (ch,))
        self.mf3 = micro(self.mf3_test020, (ch,))
        self.mf4 = micro(self.mf4_test020, (ch,))
        runnables = micro_runnables()
        self.failUnless(self.mf1 in runnables, 'mf1 not in runnables')
        self.failUnless(self.mf2 in runnables, 'mf2 not in runnables')
        self.failUnless(self.mf3 in runnables, 'mf3 not in runnables')
        self.failUnless(self.mf4 in runnables, 'mf4 not in runnables')
        self.failUnless(runnables.index(self.mf1) < runnables.index(self.mf2), 'mf1 not before mf2')
        self.failUnless(runnables.index(self.mf2) < runnables.index(self.mf3), 'mf2 not before mf3')
        self.failUnless(runnables.index(self.mf3) < runnables.index(self.mf4), 'mf3 not before mf4')
        allmicro_run()
        self.failUnlessEqual(self.ret, [1, 2, 3, 4, 5])

    def mf1_test020(self, ch):
        self.failUnless(self.ret == [], 'mf1 not started first')
        self.ret.append(1)
        secret = ch.recv(timeout=3000)
        self.failUnless(secret == 0, 'ch.recv in mf1 did not return 0 (first message sent from mf4)')
        self.failUnless(self.ret == [1, 2, 3, 4], 'mf4 did not pass to mf1')
        self.ret.append(5)

    def mf2_test020(self, ch):
        self.failUnless(self.ret != [], 'mf1 not started first')
        self.failUnless(self.ret == [1], 'mf1 ch.recv did not pass to mf2')
        runnables = micro_runnables()
        self.failUnless(self.mf2 in runnables, 'mf2 not in runnables')
        self.failUnless(self.mf3 in runnables, 'mf3 not in runnables')
        self.failUnless(self.mf4 in runnables, 'mf4 not in runnables')
        self.failUnless(runnables.index(self.mf2) < runnables.index(self.mf3), 'mf2 not before mf3')
        self.failUnless(runnables.index(self.mf3) < runnables.index(self.mf4), 'mf3 not before mf4')
        self.failUnless(self.mf1.alive, 'ch.recv in mf1, mf1 seems dead')
        self.ret.append(2)
        micro_block()

    def mf3_test020(self, ch):
        self.failUnless(self.ret != [], 'mf1 not started first')
        self.failUnless(self.ret == [1, 2], 'mf2 did not pass to mf3')
        runnables = micro_runnables()
        self.failUnless(self.mf2 in runnables, 'mf2 not in runnables')
        self.failUnless(self.mf3 in runnables, 'mf3 not in runnables')
        self.failUnless(self.mf4 in runnables, 'mf4 not in runnables')
        self.failUnless(runnables.index(self.mf3) < runnables.index(self.mf4), 'mf3 not before mf4')
        self.failUnless(runnables.index(self.mf4) < runnables.index(self.mf2), 'mf4 not before mf2')
        self.failUnless(self.mf1.alive, 'ch.recv in mf1, mf1 seems dead')
        self.ret.append(3)
        ch.send(0)

    def mf4_test020(self, ch):
        self.failUnless(self.ret == [1, 2, 3], 'mf3 did not pass to mf4')
        # If we use timeout with recv, we don't call channel.receive immediately. So when we
        #  call channel.send the schedule will pass to next tasklet normally, not to the receiver.
        self.ret.append(4)
        micro_block()

    def test110_simply_tracked_swait(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test110, (), keep_track=1)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) == 4, 'keep track 1: ' + str(log_container))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def mf1_test110(self):
        logging = ntk.lib.log.logger
        logging.debug('mf1_test110: starts, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'entering schedule')
        time_swait(300)
        logging.debug('mf1_test110: ends, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'leaving schedule')

    def test120_deeply_tracked_swait(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test110, (), keep_track=2)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) > 4, 'keep track 2: ' + str(log_container))
            self.failUnless(len(ret) % 2 == 0, 'keep track 2 non-pair: ' + str(ret))
            for i in range(len(ret) / 2):
                if not ('entering schedule' in ret[i*2] or 'leaving schedule' in ret[i*2+1]):
                    self.fail('keep track 2 wrong order: ' + str(ret))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def test130_no_tracked_swait(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test110, ())
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) == 2, 'no keep track: ' + str(log_container))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def test140_simply_tracked_condition(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test140, (), keep_track=1)
            self.mf2 = micro(self.mf2_test140, (), keep_track=1)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) == 10, 'keep track 1: ' + str(log_container))
            for i in range(len(ret) / 2):
                if not ('entering schedule' in ret[i*2] or 'leaving schedule' in ret[i*2+1]):
                    self.fail('keep track 1 wrong order: ' + str(ret))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def mf1_test140(self):
        logging = ntk.lib.log.logger
        logging.debug('mf1_test140: starts, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'entering schedule')
        logging.debug('mf1_test140: wait')
        time_swait(100)
        self.semaphore = False
        logging.debug('mf1_test140: while_condition')
        def f():
            return self.semaphore
        time_while_condition(f)
        logging.debug('mf1_test140: ends, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'leaving schedule')

    def mf2_test140(self):
        logging = ntk.lib.log.logger
        logging.debug('mf2_test140: starts, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'entering schedule')
        logging.debug('mf2_test140: wait more')
        time_swait(200)
        self.semaphore = True
        logging.debug('mf2_test140: ends, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'leaving schedule')

    def test150_deeply_tracked_condition(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test140, (), keep_track=2)
            self.mf2 = micro(self.mf2_test140, (), keep_track=2)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) > 10, 'keep track 2: ' + str(log_container))
            for i in range(len(ret) / 2):
                if not ('entering schedule' in ret[i*2] or 'leaving schedule' in ret[i*2+1]):
                    self.fail('keep track 2 wrong order: ' + str(ret))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def test160_simply_tracked_kill(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test160, (), keep_track=1)
            self.mf2 = micro(self.mf2_test160, (self.mf1,), keep_track=1)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) == 8, 'keep track 1: ' + str(log_container))
            for i in range(len(ret) / 2):
                if not ('entering schedule' in ret[i*2] or 'leaving schedule' in ret[i*2+1]):
                    self.fail('keep track 1 wrong order: ' + str(ret))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

    def mf1_test160(self):
        logging = ntk.lib.log.logger
        logging.debug('mf1_test160: starts, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'entering schedule')
        time_swait(300)
        logging.debug('should not appear')
        # When a tasklet is killed, it does not enter schedule, it just dies.
        # So we don't log 'leaving schedule' at the end.

    def mf2_test160(self, other):
        logging = ntk.lib.log.logger
        logging.debug('mf2_test160: starts, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'entering schedule')
        time_swait(200)
        micro_kill(other)
        logging.debug('mf2_test160: ends, it is an implicit scheduling, so...')
        logging.log(logging.ULTRADEBUG, 'leaving schedule')

    def test170_deeply_tracked_kill(self):
        global log_on_console, log_container
        try:
            log_on_console_was, log_container_was = log_on_console, log_container

            log_on_console = False
            log_container = []
            self.mf1 = micro(self.mf1_test160, (), keep_track=2)
            self.mf2 = micro(self.mf2_test160, (self.mf1,), keep_track=2)
            allmicro_run()
            ret = [log for log in log_container \
                         if 'leaving schedule' in log \
                         or 'entering schedule' in log]
            self.failUnless(len(ret) > 8, 'keep track 1: ' + str(log_container))
            for i in range(len(ret) / 2):
                if not ('entering schedule' in ret[i*2] or 'leaving schedule' in ret[i*2+1]):
                    self.fail('keep track 1 wrong order: ' + str(ret))

        finally:
            log_on_console, log_container = log_on_console_was, log_container_was

if __name__ == '__main__':
    unittest.main()

