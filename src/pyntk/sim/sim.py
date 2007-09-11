##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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


sys.path.append('..')
from lib.micro import Channel

class Event(object):
    __slots__ = [ 'exec_time', 'callf', 'abs_time' ]

    def __init__(self, time, callf, callf_args):
	"""time: how much time needs this event to be executed
	   callf: the callback function called when the event is executed
	   callf_args: arguments passed to callf"""
	self.callf     = callf
	self.callf_args= callf_args
        self.exec_time = time
	
	# absolute time: this is set when the event is added in the queue
	self.abs_time=0
    
    def __cmp__(self, other):
        return cmp(self.abs_time, other.abs_time)

class Simulator(object):
    """This is a Descrete Event Simulator"""

    def __init__(self, tickfreq=50):
        """tickfreq: tick frequency in millisecond"""
        
	self.tickfreq = tickfreq
        
        self.curtime = 0
	self.queue = []
    
    def ev_add(self, ev):
        ev.abs_time = self.curtime+ev.exec_time
        heappush(self.queue, ev)

    def ev_exec(self):
	ev = heappop(self.queue)
	ev.callf(*ev.callf_args)
	self.curtime = ev.abs_time

    def loop(self):
        while self.queue != []:
		self.ev_exec()

    def _wake_up_wait(self, chan):
        chan.send(())

    def wait(self, t):
        """Waits the specified number of time units"""
	chan = Channel()
	self.ev_add( Event(t, self._wake_up_wait, chan) )
	chen.recv()
