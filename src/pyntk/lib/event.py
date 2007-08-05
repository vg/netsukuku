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


class EventError(Exception):pass

class Event:
	def __init__(self, events):
		self.events    = events	  # List of events
		self.listeners = {}	  # {event : [listener,] }
		
	def send(self, event, msg):
		if event not in self.events:
			raise EventError, "\""+event+"\" is not a registered event"

		if self.listeners.has_key(event):
			for dst in self.listeners[event]:
				self._send(dst, msg)

	def _send(self, dst, msg):
		# TODO
		# What to use? tasklets? queue.append()? 
		# Surely it must not block
		pass

	def listen(self, event, dst, remove=0):
		"""Listen to an event.

		If `remove'=0, then `dst' is added in the listeners queue of
		the event `event', otherwise `dst' is removed from that queue.
		"""

		if event not in self.events:
			raise EventError, "\""+event+"\" is not a registered event"

		if event not in self.listeners:
			self.listeners[event]=[]

		if not remove:
			self.listeners[event].append(dst)
		else if dst in self.listeners[event]:
			self.listeners[event].remove(dst)
