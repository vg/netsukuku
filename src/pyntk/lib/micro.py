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

def dispatcher(func, chan, is_micro):
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
    def decorate(func)
        ch = Channel()
        micro(dispatcher, (func, ch, is_micro))
        return ch.send

    return decorate
