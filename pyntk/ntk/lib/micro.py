##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

from ntk.lib.log import logger as logging
from ntk.lib.log import log_exception_stacktrace
import stackless
import functools

def micro(function, args=(), **kwargs):
    '''Factory function that returns tasklets

    @param function: A callable
    @return: A tasklet
    '''
    t = stackless.tasklet()
 
    def callable():
        try:
            function(*args, **kwargs)
        except Exception, e:
            logging.error("Uncaught exception in a microfunc")
            logging.error("  The microfunc has been called like this: %s(%s,%s)" % (function.__name__, args.__repr__(), kwargs.__repr__()))
            log_exception_stacktrace(e)

    t.bind(callable)
    return t()

def microatomic(function, args=(), **kwargs):
    '''Factory function that returns atomic tasklets, 
    usable only with preemptive schedulers.
 
    @param function: A callable
    @return: A tasklet
    '''
    t = stackless.tasklet()
 
    def callable():
        flag = t.set_atomic(True)
        try:
            function(*args, **kwargs)
        finally:
            t.set_atomic(flag)
 
    t.bind(callable)
    return t()

def micro_block():
    stackless.schedule()

def allmicro_run():
    stackless.run()


class Channel(object):
    '''This class is used to wrap a stackless channel'''
    __slots__ = ['ch', 'chq', 'micro_send']

    def __init__(self, prefer_sender=False, micro_send=False):
        """If prefer_sender=True, the send() calls won't block. More
        precisely, the calling tasklet doesn't block and the receiving tasklet
        will be the next to be scheduled.

        If micro_send=True, then a new microthread will be used for each
        send call, thus each send() call won't block.
        """
        self.ch  = stackless.channel()
        self.chq = []
        self.micro_send = micro_send
        if prefer_sender:
            self.ch.preference = 1

    def send(self, data):
        if self.micro_send:
            micro(self.ch.send, (data,))
        else:
            self.ch.send(data)

    def send_exception(self, exc, value, wait=False):
        result = None
        for i in range(0, self.ch.balance, -1):
            # there are tasklets waiting to receive
            result = stackless.channel.send_exception(self.ch, exc, value)
        return result

    def recv(self):
        return self.ch.receive()

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
           
           This is best used in a Channel with prefer_sender=True and micro_send=False
        '''
        while self.ch.balance < 0:
            self.ch.send(data)

def _dispatcher(func, chan):
    while True:
        msg = chan.recvq()
        try:
            func(*msg)
        except Exception, e:
            logging.error("Uncaught exception in a microfunc with dispatcher")
            logging.error("  The microfunc has been called like this: %s(%s)" % (func.__name__, msg.__repr__()))
            log_exception_stacktrace(e)

def microfunc(is_micro=False, is_atomic=False):
    '''A microfunction is a function that never blocks the caller microthread.

    Note: This is a decorator! (see test/test_micro.py for examples)

    If is_micro != True and is_micro != True (default), each call will be queued. 
    A dispatcher microthread will automatically pop and execute each call.
    If is_micro == True, each call of the function will be executed in a new
    microthread. 
    If is_atomic == True, each call will be executed inside a new atomic
    microthread. WARNING: this means that the microthread won't be interrupted
    by the stackless scheduler until it has finished running.
    '''

    def decorate(func):
        ch = Channel(True)

        @functools.wraps(func)
        def fsend(*data):
            ch.sendq(data)

        @functools.wraps(func)
        def fmicro(*data, **kwargs):
            micro(func, data, **kwargs)

        @functools.wraps(func)
        def fatom(*data, **kwargs):
            microatomic(func, data, **kwargs)
 
        if is_atomic:
            return fatom
        elif is_micro:
            return fmicro
        else:
            micro(_dispatcher, (func, ch))
            return fsend

    return decorate
