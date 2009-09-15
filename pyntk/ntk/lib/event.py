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

from ntk.lib.log import logger as logging
from ntk.lib.log import get_stackframes
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
    
    class Channel_with_wakeup(Channel):
        def __init__(self):
            Channel.__init__(self, prefer_sender=True)

        ## Create the dispatcher and the event_wait() function. They are all
        ## dependent on this Channel

        def _wakeup_on_event_dispatcher(self, *event_data):
            logging.debug('I will send a wakeup on chan ' + str(self))
            stacktrace = get_stackframes(back=1)
            logging.debug('  the event above was called from ' + stacktrace)
            self.__wakeup_on_event_dispatcher(*event_data)

        @microfunc()   # Each call is queued. No event will be lost
        def __wakeup_on_event_dispatcher(self, *event_data):
            self.bcast_send(event_data)  # blocks if necessary
            logging.debug('wakeup sent on chan ' + str(self))

        def event_wait_func(self):
            logging.debug('wakeup receiving on chan ' + str(self))
            stacktrace = get_stackframes(back=1)
            logging.debug('  the event_wait above was called from ' + stacktrace)
            ret = self.recv()
            logging.debug('wakeup received on chan ' + str(self))
            logging.debug('  the event_wait above was called from ' + stacktrace)
            return ret

    def decorate(func):
        event_wait_func_dict={ }  # { (ev, evname) : wait_func }
        for ev, evname in events:
                chan = Channel_with_wakeup()
                logging.debug('wakeup decorator on func ' + str(func) + ': created chan for ' + evname + ', chan ' + str(chan))
                stacktrace = get_stackframes(back=1)
                if stacktrace.find('apply_wakeup_on_event') >= 0: stacktrace = get_stackframes(back=2)
                logging.debug('  the decoration above was called from ' + stacktrace)

                # Register _wakeup_on_event_dispatcher as a listener of the specified event
                ev.listen(evname, chan._wakeup_on_event_dispatcher)
                ##

                event_wait_func_dict[(ev, evname)] = chan.event_wait_func

        func_with_wait=functools.partial(func, event_wait=event_wait_func_dict)
        
        # if the method must be remotable (and used by rpc), we need 
        # to add this attributes to the functools.partial function
        class fake(): 
            def func(self): pass
            
        setattr(func_with_wait, '__name__', func.__name__)
        if hasattr(func, 'im_func'):
            setattr(func_with_wait, 'im_func', func.im_func)
        else:
            setattr(func_with_wait, 'im_func', fake().func.im_func)
        ##
            
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
