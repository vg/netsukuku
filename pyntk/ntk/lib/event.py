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
    """Event class.

    Any module M can register its own events. Any other module N can
    listen to these events.

    ** Registering the events

            class M:
                def __init__(self):
                        self.events = Event( ['EVENT1', 'EVENT2', 'FOOEVENT'] )
                        ...
                        self.events.add( ['EVENT3', 'EVENT4'] )

    ** Sending an event:

            class M:
                ...
                def f(self):
                    self.events.send('EVENT1', (param1, param2, param3))

    ** Listening to an event

            class N:
                def __init__(self, m)
                    # m is an instance of the class M
                    m.events.listen('EVENT1', self.func_for_ev1)

                def func_for_ev1(param1, param2, param3):
                    ...

        If func_for_ev1 is defined as a normal function, then the code calling
        events.send('EVENT1', (...)) will block until func_for_ev1 finishes.
        If you don't want this behaviour, then the listeners must be
        microfunctions, f.e.

                @microfunc()
                def func_for_ev1(param1, param2, param3):
                    ...

        For more info on microfunctions see lib/micro.py
    """

    def __init__(self, events):
        self.events    = events   # List of events
        self.listeners = {}       # {event : [listener,] }

    def add(self, events):
        self.events += events

    def send(self, event, msg):
        if event not in self.events:
            raise EventError, "\""+event+"\" is not a registered event"

        if event in self.listeners:
            for dst in self.listeners[event]:
                self._send(dst, msg)

    def _send(self, dst, msg):
        # call the microfunc `dst'
        dst(*msg)

    def listen(self, event, dst, remove=False):
        """Listen to an event.

        If `remove'=False, then `dst' is added in the listeners queue of
        the event `event', otherwise `dst' is removed from that queue.
        """

        if event not in self.events:
            raise EventError, "\""+event+"\" is not a registered event"

        if event not in self.listeners:
            self.listeners[event]=[]

        if not remove:
            self.listeners[event].append(dst)
        elif dst in self.listeners[event]:
            self.listeners[event].remove(dst)
