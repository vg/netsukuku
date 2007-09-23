##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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

import asyncore
import socket

import stackless
import functools

def micro(function, args=()):
    '''Factory function that return tasklets

    @param function: A callable
    @return: A tasklet
    '''
    return stackless.tasklet(function)(*args)

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
	self.micro_send=micro_send
	if prefer_sender:
		self.ch.preference=1

    def send(self, data):
        if self.micro_send:
		micro(self.ch.send, (data,))
	else:
		self.ch.send(data)
    
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

def _dispatcher(func, chan):
    while True:
        msg = chan.recvq()
        func(*msg)

def microfunc(is_micro=False):
    '''A microfunction is a function that never blocks the caller microthread.

    Note: This is a decorator! (see test/test_micro.py for examples)

    If is_micro == True, each call of the function will executed in a new
    microthread. 
    If is_micro != True, each call will be queued. A dispatcher microthread
    will automatically pop and execute each call.
    '''

    def decorate(func):
        ch = Channel(True)

	@functools.wraps(func)
        def fsend(*data):
            ch.sendq(data)
        
	@functools.wraps(func)
        def fmicro(*data):
            micro(func, data)

	if is_micro:
		return fmicro
	else:
		micro(_dispatcher, (func, ch))
		return fsend

    return decorate
