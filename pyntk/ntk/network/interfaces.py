# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
    '''Rapresents a Network Iterface Controller'''

    def __init__(self, name):
        self.name = name

    def up(self):
        '''Brings the interface up.'''
        raise NotImplementedError

    def down(self):
        '''Brings the interface down.'''
        raise NotImplementedError

    def show(self):
        '''Shows NIC information.'''
        raise NotImplementedError

    def flush(self):
        '''Flushes all NIC addresses.'''
        raise NotImplementedError

    def set_address(self, address):
        '''Set NIC address.'''
        raise NotImplementedError

    def get_address(self):
        '''Gets NIC address.'''
        raise NotImplementedError

    def _get_address_getter(self):
        return self.get_address()

    def _set_address_getter(self, address):
        return self.set_address(address)

    address = property(_get_address_getter, _set_address_getter)

    def get_mac(self):
        '''Gets NIC MAC address'''
        raise NotImplementedError

    def _get_mac_getter(self):
        return self.get_mac()

    mac = property(_get_mac_getter)

    def get_is_active(self):
        '''Returns True if NIC is active.'''
        raise NotImplementedError

    def _get_is_active_getter(self):
        return self.get_is_active()

    is_active = property(_get_is_active_getter)

    # multicast is used with ipv6
    def set_multicast(self, m):
        '''Set multicast for the interface.'''
        raise NotImplementedError

    def get_multicast(self, m):
        '''Gets multicast for the interface.'''
        raise NotImplementedError

    def _get_multicast_getter(self):
        return self.get_multicast()

    def _set_multicast_getter(self, m):
        return self.set_multicast(self, m)

    multicast = property(_get_multicast_getter, _set_multicast_getter)

    def filtering(self, *args, **kargs):
        '''Enables or disables filtering.'''
        raise NotImplementedError

    def __str__(self):
        return '<NIC: %s>' % self.name

    def __repr__(self):
        return '<NIC: %s>' % self.name


class BaseRoute(object):
    '''Rapresents a route controller.'''

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None):
        '''Adds a new route with corresponding properties.'''
        raise NotImplementedError

    @staticmethod
    def change(properties):
        '''Edits the route with the corresponding properties.'''
        raise NotImplementedError

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None):
        '''Removes the route with the corresponding properties.'''
        raise NotImplementedError

    @staticmethod
    def flush():
        '''Flushes all routes.'''
        raise NotImplementedError

    @staticmethod
    def flush_cache():
        '''Flushes cache'''
        raise NotImplementedError

    @staticmethod
    def ip_forward(enable):
        '''Enables/disables ip forwarding.'''
        raise NotImplementedError
