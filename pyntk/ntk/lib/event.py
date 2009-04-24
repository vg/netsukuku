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

from micro import Channel, microfunc
import functools

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

def wakeup_on_event(events=[]):
    '''This is a decorator. A function decorated with wakeup_on_event() can
    call event_wait() in order to block and wait for the specified events.

    events=[(EventInstance, EventName), ...]
    EventClass is an Event() instance, EventName is a string.
    
    The decorated function must define the keyword argument `event_wait' (with
    arbitrary initial value).

    F.e. 
        @wakeup_on_event(events=[(wheater_events, 'IT_IS_SUNNY')])
        def go_out(param1, param2, event_wait=None):
                ...
                
                msg = event_wait[(wheater_events, 'IT_IS_SUNNY')]() # Block and wait the event 'IT_IS_SUNNY'

                # Ok, here I've received the event 'IT_IS_SUNNY'. `msg' is the
                # data sent along with the event
                do_stuff()
                ...
    Note: event_wait is a dictionary of the following type: { (event_instance, evname) : wait_func }
    '''

    def decorate(func):
        event_wait_func_dict={ }  # { (ev, evname) : wait_func }
        for ev, evname in events:
                chan = Channel(prefer_sender=False)
                ## Create the dispatcher and the event_wait() function. They are all
                ## dependent on `chan'
                ##

                @microfunc(is_micro=False) # Using is_micro=False, ensures that each
                                           # call is queued. In this way, no event
                                           # will be lost
                def _wakeup_on_event_dispatcher(*event_data):
                        chan.bcast_send(event_data)  # blocks if necessary

                def event_wait_func():
                        return chan.recv()

                # Register _wakeup_on_event_dispatcher as a listener of the specified event
                ev.listen(evname, _wakeup_on_event_dispatcher)
                ##

                event_wait_func_dict[(ev, evname)]=event_wait_func

        func_with_wait=functools.partial(func, event_wait=event_wait_func_dict)
        return func_with_wait

    return decorate

def apply_wakeup_on_event(func, events=[]):
        '''This is the same of wakeup_on_event(), but it has to be manually
        applied. F.e:
                def go_out(param1, param2, event_wait=None):
                        ... # the same as the example of wakeup_on_event()
                go_out=apply_wakeup_on_event(go_out, events=[(wheater_events, 'IT_IS_SUNNY')])
        '''
                
        @wakeup_on_event(events)
        @functools.wraps(func)
        def new_func(*args, **kwargs):
                return func(*args, **kwargs)
        return new_func
