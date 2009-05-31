##
# This file is part of Netsukuku
# (c) Copyright 2009 Daniele Tricoli aka Eriol <eriol@mornie.org>
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


class ObserverBlindError(Exception):
    '''An eye for eye only ends up making the whole world blind.'''

class BaseObserver(object):

    def __init__(self, who=None, what=None):
        '''A generic observer with auto-generated name.

        :param who: who is observed
        :param what: events we want to observe
        :type what: list of strings

        You must derive your observer from this class and provide a callback
        method for each event observed.
        Callback name must be equal to event name, but in lower case.
        Put data associated with event inside self.<event_name>_event.
        '''

        if who is None or what is None:
            raise ObserverBlindError

        def _fake_method(*args, **kwargs):
            raise NotImplementedError

        for event in what:
            # Set self.<event_name>_event = None
            # E.g:
            #     Given the event 'BLINDED_BY_THE_LIGHT' you will have
            #     self.blinded_by_the_light_event = None
            setattr(self, event.lower() + '_event', None)

            # Try accessing to self.<event_name>: it represents the callback
            # for event <EVENT_NAME>. If it's not defined, set _fake_method
            # as callback function.
            try:
                getattr(self, event.lower())
            except AttributeError:
                setattr(self, event.lower(), _fake_method)

            who.events.listen(event, getattr(self, event.lower()))
