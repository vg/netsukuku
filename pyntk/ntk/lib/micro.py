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
import time

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

def micro_block():
    stackless.schedule()

def allmicro_run():
    stackless.run()


class MicrochannelTimeout(Exception):
    pass

class Channel(object):
    '''This class is used to wrap a stackless channel'''
    __slots__ = ['ch', 'chq', 'micro_send', 'balance', '_balance_receiving', '_balance_sending']

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
            self.ch.send(data)

    def send_exception(self, exc, value, wait=False):
        result = None
        for i in range(0, self.ch.balance, -1):
            # there are tasklets waiting to receive
            result = stackless.channel.send_exception(self.ch, exc, value)
        return result

    def recv(self, timeout=None):
        if timeout is not None:
            try:
                self._balance_receiving = True
                expires = time.time() + timeout/1000
                while self.ch.balance <= 0 and expires > time.time():
                    time.sleep(0.001)
                    micro_block()
                if self.ch.balance > 0:
                    return self.ch.receive()
                else:
                    raise MicrochannelTimeout()
            finally:
                self._balance_receiving = False
        else:
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
            logging.error("Uncaught exception in a microfunc with dispatcher")
            logging.error("  The microfunc has been called like this: %s(%s)" % (func.__name__, msg.__repr__()))
            log_exception_stacktrace(e)

def microfunc(is_micro=False, dispatcher_token=DispatcherToken()):
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
            micro(func, data, **kwargs)

        if is_micro:
            return fmicro
        else:
            micro(_dispatcher, (func, ch, dispatcher_token))
            return fsend

    return decorate
