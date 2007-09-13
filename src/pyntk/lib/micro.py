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

class Micro(object):

    def __init__(self, function, *args):
        self.function = function
        self.args = args

    def micro(self):
        if self.args:
            return stackless.tasklet(self.function)(args)()
        else:
            return stackless.tasklet(self.function)()

class Channel(object):
    '''This class is used to wrap a stackless channel'''

    def __init__(self):
        self.ch = stackless.channel()

    def send(self, data):
        self.ch.send(data)

    def recv(self):
        return self.ch.receive()


def micro(function, *args):
    '''Factory function that return tasklets

    @param function: A callable
    @return: A tasklet
    '''
    if args:
        return stackless.tasklet(function)(args)()
    else:
        return stackless.tasklet(function)()

def _dispatcher(func, chan, is_micro):
    while True:
        msg = chan.recv()
    if is_micro:
        micro(func, msg)
    else:
        func(*msg)

def microfunc(is_micro=False):
    '''Create a new channel and start a microthread

    This is a decorator

    @param is_micro: Tells the dispatcher to create a new microthread

    '''
    def decorate(func):
        ch = Channel()
        micro(_dispatcher, (func, ch, is_micro))
        return ch.send

    return decorate

class SocketScheduler(object):
    '''A socket scheduler using asyncore module'''
    def __init__(self):
        self.running = False

    def sched(self):
        while True:
            if not asyncore.socket_map:
                break

            asyncore.poll(0.0)
            stackless.schedule()

        self.running = False

def socket_factory(scheduler,
                   family=socket.AF_INET,
                   type=socket.SOCK_STREAM,
                   proto=0):

    s = socket.socket(family, type, proto)
    if not scheduler.running:
        scheduler.running = True
        stackless.tasklet(scheduler.sched)()
    return SocketDispatcher(s)


class SocketDispatcher(asyncore.dispatcher):
    '''A socket dispatcher using asyncore

    Based on stacklesssocket.py of Richard Tew
    '''
    def __init__(self, sock):
        asyncore.dispatcher.__init__(self, sock)

        self.accept_channel = stackless.channel()
        self.connect_channel = stackless.channel()
        self.read_channel = stackless.channel()
        self.input_buffer = ''
        self.output_buffer = ''

    def accept(self):
        return self.accept_channel.receive()

    def connect(self, address):
        asyncore.dispatcher.connect(self, address)
        if not self.connected:
            self.connect_channel.receive()

    def send(self, data):
        self.output_buffer += data
        return len(data)

    def recv(self, buffer_size):
        if len(self.input_buffer) < buffer_size:
            self.input_buffer += self.read_channel.receive()
        data = self.input_buffer[:buffer_size]
        self.input_buffer = self.input_buffer[buffer_size:]
        return data

    def close(self):
        asyncore.dispatcher.close(self)
        self.connected = False
        self.accepting = False

        # Errors handling
        while self.accept_channel.balance < 0:
            self.accept_channel.send_exception(
                                    socket.error, 9, 'Bad file descriptor')
        while self.connect_channel.balance < 0:
            self.connect_channel.send_exception(
                                    socket.error, 10061, 'Connection refused')
        while self.read_channel.balance < 0:
            self.read_channel.send_exception(
                                    socket.error, 10054, 'Connection reset by peer')

    def wrap_accept_socket(self, currentSocket):
        return SocketDispatcher(currentSocket)

    def handle_accept(self):
        if self.accept_channel.balance < 0:
            sock, address = asyncore.dispatcher.accept(self)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock = self.wrap_accept_socket(sock)
            stackless.tasklet(self.accept_channel.send)((sock, address))

    def handle_connect(self):
        self.connect_channel.send(None)

    def handle_close(self):
        self.close()

    def handle_expt(self):
        self.close()

    def handle_read(self):
        buffer = asyncore.dispatcher.recv(self, 20000)
        stackless.tasklet(self.read_channel.send)(buffer)

    def handle_write(self):
        if len(self.output_buffer):
            sent_bytes = asyncore.dispatcher.send(
                                        self, self.output_buffer[:512])
            self.output_buffer = self.output_buffer[sent_bytes:]
