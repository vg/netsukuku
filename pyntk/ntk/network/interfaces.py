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


class BaseNIC(object):
    ''' Rapresents a Network Iterface Controller '''

    def __init__(self, name):
        self.name = name

    def up(self):
        ''' Brings the interface up '''
        raise NotImplementedError

    def down(self):
        ''' Brings the interface down '''
        raise NotImplementedError

    def show(self):
        ''' Shows NIC address information   '''
        raise NotImplementedError

    def set_address(self, address):
        ''' Set NIC address '''
        raise NotImplementedError

    def get_address(self):
        ''' Gets NIC address '''
        raise NotImplementedError

    address = property(get_address, set_address)

    # TODO: we really use multicast?
    def set_multicast(self, m):
        ''' Set multicast for the interface '''
        raise NotImplementedError

    def get_multicast(self, m):
        ''' Gets multicast for the interface '''
        raise NotImplementedError

    multicast = property(get_multicast, set_multicast)

class BaseRoute(object):
    ''' Rapresents a route '''

    def add(self, properties):
        ''' Adds a new route with corresponding properties '''
        raise NotImplementedError

    def edit(self, properties):
        ''' Edits the route with the corresponding properties '''
        raise NotImplementedError

    def delete(self, properties):
        ''' Removes the route with the corresponding properties '''
        raise NotImplementedError

    def flush(self, properties):
        ''' Flushes routes selected by some criteria '''
        raise NotImplementedError

    def flush_cache(self, properties):
        '''  '''
        raise NotImplementedError

    def ip_forward(self, enable):
        ''' Enables/disables ip forwarding '''
        raise NotImplementedError
