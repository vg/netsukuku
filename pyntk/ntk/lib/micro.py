##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

import functools
import stackless
import time

from ntk.lib.log import logger as logging
from ntk.lib.log import log_exception_stacktrace, get_stackframes
from ntk.lib.log import ExpectableException

keep_track_prog = 0
keep_track_map = {}
keep_track_extended_map = {}

def micro(function, args=(), keep_track=0, **kwargs):
    '''Factory function that returns tasklets

    keep_track is used to specify that we want to log
     every time the tasklet leaves or enters the schedule.
     Set to 1 to log normally. It tries to not log the
      times the tasklet is scheduled but does nothing
      because it is in a wait or in a Channel operation
     Set to 2 to log alot

    @param function: A callable
    @param keep_track: an int
    @return: A tasklet
    '''
    t = stackless.tasklet()
 
    def callable():
        try:
            function(*args, **kwargs)
        except Exception, e:
            if isinstance(e, ExpectableException):
                logging.debug(str(e) + ' raised in ' \
                              + str(function) + str(args))
            else:
                logging.error("Uncaught exception in a microfunc")
                logging.error("  The microfunc has been called like this: "
                "%s(%s,%s)" % (function.__name__, args.__repr__(), 
                               kwargs.__repr__()))
                log_exception_stacktrace(e)

    t.bind(callable)
    if keep_track > 0:
        global keep_track_prog, keep_track_map, keep_track_extended_map
        id_tsk = keep_track_prog = keep_track_prog + 1
        if keep_track > 1:
            keep_track_extended_map[t] = id_tsk
        else:
            keep_track_map[t] = id_tsk
        logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
                    str(id_tsk) + ' created, will start soon. ' +\
                    str(function) + str(args))
        micro(task_surveillor, (t, id_tsk))
    t()
    return t

def start_tracking(keep_track=1):
    global keep_track_prog, keep_track_map, keep_track_extended_map
    t = micro_current()
    if t in keep_track_extended_map or \
            t in keep_track_map:
        #akready tracking this tasklet
        return
    if keep_track > 0:
        id_tsk = keep_track_prog = keep_track_prog + 1
        if keep_track > 1:
            keep_track_extended_map[t] = id_tsk
        else:
            keep_track_map[t] = id_tsk
        logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
                    str(id_tsk) + ' now registered for tracking, and being in schedule. ' +\
                    get_stackframes())

def stop_tracking():
    global keep_track_map, keep_track_extended_map
    tsk = micro_current()
    id_tsk = 0 # no task
    if tsk in keep_track_extended_map:
        id_tsk = keep_track_extended_map[tsk]
        del keep_track_extended_map[tsk]
    if tsk in keep_track_map:
        id_tsk = keep_track_map[tsk]
        del keep_track_map[tsk]
    if id_tsk > 0:
        logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
                str(id_tsk) + ' now unregistered from tracking.')

def _scheduling(enter, extended):
    global keep_track_map, keep_track_extended_map
    tsk = micro_current()
    id_tsk = 0 # no task
    if extended >= 0:
        if tsk in keep_track_extended_map:
            id_tsk = keep_track_extended_map[tsk]
    if extended <= 0 and id_tsk == 0:
        if tsk in keep_track_map:
            id_tsk = keep_track_map[tsk]
    if id_tsk > 0:
        action = ' entering schedule.' if enter else ' leaving schedule.'
        logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
                    str(id_tsk) + action)

def scheduling():
    _scheduling(enter=True, extended=0)

def scheduling_ext():
    _scheduling(enter=True, extended=1)

def scheduling_no_ext():
    _scheduling(enter=True, extended=-1)

def leaving():
    _scheduling(enter=False, extended=0)

def leaving_ext():
    _scheduling(enter=False, extended=1)

def leaving_no_ext():
    _scheduling(enter=False, extended=-1)

def task_surveillor(tsk, id_tsk):
    logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
            str(id_tsk) + ' started.')
    while True:
        time.sleep(0.001)
        micro_block()
        if not tsk.alive:
            logging.log(logging.ULTRADEBUG, 'micro: tasklet ' +\
                    str(id_tsk) + ' terminated a short while ago.')
            return

def micro_block():
    leaving()
    stackless.schedule()
    scheduling()

def allmicro_run():
    stackless.run()

def micro_kill(tasklet):
    leaving()
    tasklet.kill()
    scheduling()

def micro_current():
    return stackless.getcurrent()

def micro_runnables():
    ret = []
    cur = stackless.getcurrent()
    ret.append(cur)
    next = cur.next
    while next is not cur:
        ret.append(next)
        next = next.next
    return ret

# Act as a provider to services that hide the recurring scheduling
#  of this tasklet. E.g. "swait"

def time_swait(t):
    """Waits `t' ms"""
    _time_swait(t, True, True)

def _time_swait(t, always_first, always_last):
    """Waits `t' ms"""

    final_time = time.time()+t/1000.
    first = True
    while True:
        if final_time < time.time():
            if not first:
                if always_last: scheduling()
                else: scheduling_ext()
            break
        else:
            if not first:
                scheduling_ext()
        time.sleep(0.001)
        if first:
            if always_first: leaving()
            else: leaving_ext()
            first = False
        else:
            leaving_ext()
        stackless.schedule()

def time_while_condition(func, wait_millisec=10, repetitions=0):
    """If repetitions=0, it enters in an infinite loop checking each time
       if func()==True, in which case it returns True.
    If repetitions > 0, then it iterates the loop for N times, where
    N=repetitions. If func() failed to be True in each iteration, False is
    returned instead.

    :param wait_millisec: number of milliseconds to wait at the end of
                          each iteration.
    """

    i = 0
    first = True
    while True:
        if func():
            if not first:
                _time_swait(1, False, True)
            return True
        _time_swait(wait_millisec, first, False)
        first = False
        if not repetitions: continue
        i += 1
        if i >= repetitions:
            _time_swait(1, False, True)
            return False

#

class MicrochannelTimeout(Exception):
    pass

class Channel(object):
    '''This class is used to wrap a stackless channel'''
    __slots__ = ['ch', 'chq', 'micro_send', 'balance', '_balance_receiving', 
                 '_balance_sending']

    def __init__(self, prefer_sender=False, micro_send=False):
        """If prefer_sender=True, the send() calls won't block. More
        precisely, the calling tasklet doesn't block and the receiving tasklet
        will be the next to be scheduled.

        If micro_send=True, then a new microthread will be used for each
        send call, thus each send() call won't block.
        """
        self.ch  = stackless.channel()
        self._balance_receiving = False
        self._balance_sending = False
        self.chq = []
        self.micro_send = micro_send
        if prefer_sender:
            self.ch.preference = 1

    def get_balance(self):
        if self._balance_sending:
            return self.ch.balance + 1
        if self._balance_receiving:
            return self.ch.balance - 1
        return self.ch.balance

    def _get_balance_getter(self):
        return self.get_balance()

    balance = property(_get_balance_getter)

    def send(self, data):
        if self.micro_send:
            micro(self.ch.send, (data,))
        else:
            leaving()
            self.ch.send(data)
            scheduling()

    def send_exception(self, exc, value, wait=False):
        result = None
        for i in range(0, self.ch.balance, -1):
            # there are tasklets waiting to receive
            result = stackless.channel.send_exception(self.ch, exc, value)
        return result

    def recv(self, timeout=None):
        if timeout is not None:
            leaving_no_ext()
            try:
                self._balance_receiving = True
                expires = time.time() + timeout/1000
                while self.ch.balance <= 0 and expires > time.time():
                    time.sleep(0.001)
                    leaving_ext()
                    stackless.schedule()
                    scheduling_ext()
                if self.ch.balance > 0:
                    return self.ch.receive()
                else:
                    raise MicrochannelTimeout()
            finally:
                self._balance_receiving = False
                scheduling_no_ext()
        else:
            leaving()
            ret = self.ch.receive()
            scheduling()
            return ret

    def sendq(self, data):
        """It just sends `data' to the channel queue.
           `data' can or cannot be received."""
        if self.ch.balance < 0:
            self.send(data)
        else:
            self.chq.append(data)

    def recvq(self):
        """Receives data sent by `sendq'"""
        if self.chq == []:
            return self.recv()
        else:
            return self.chq.pop(0)

    def bcast_send(self, data):
        '''Send `data' to _all_ tasklets that are waiting to receive.
           If there are no tasklets, this function will immediately return!
           
           This is best used in a Channel with prefer_sender=True 
           and micro_send=False
        '''
        while self.ch.balance < 0:
            leaving()
            self.ch.send(data)
            scheduling()

class DispatcherToken(object):
    def __init__(self):
        self.executing = False

def _dispatcher(func, chan, dispatcher_token):
    while True:
        dispatcher_token.executing = False
        msg = chan.recvq()
        dispatcher_token.executing = True
        try:
            func(*msg)
        except Exception, e:
            if isinstance(e, ExpectableException):
                logging.debug(str(e) + ' raised in ' \
                              + str(func) + str(msg))
            else:
                logging.error("Uncaught exception in a microfunc with dispatcher")
                logging.error(" The microfunc has been called like this: %s(%s)" %
                              (func.__name__, msg.__repr__()))
                log_exception_stacktrace(e)

def microfunc(is_micro=False, keep_track=0, dispatcher_token=DispatcherToken()):
    '''A microfunction is a function that never blocks the caller microthread.

    Note: This is a decorator! (see test/test_micro.py for examples)

    If is_micro != True (default), each call will be queued.
    A dispatcher microthread will automatically pop and execute each call.

    If is_micro == True, each call of the function will be executed in a new
    microthread.
    
    When declaring a microfunc with dispatcher (is_micro == False) an instance
    of DispatcherToken can be passed. It will permit to see in any moment if
    the dispatcher is serving a request.
    '''

    def decorate(func):
        ch = Channel(True)

        @functools.wraps(func)
        def fsend(*data):
            ch.sendq(data)

        @functools.wraps(func)
        def fmicro(*data, **kwargs):
            micro(func, data, keep_track=keep_track, **kwargs)

        if is_micro:
            return fmicro
        else:
            micro(_dispatcher, (func, ch, dispatcher_token), keep_track=keep_track)
            return fsend

    return decorate
