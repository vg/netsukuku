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

from ntk.lib.log import logger as logging

class BaseNIC(object):
    ''' Rapresents a Network Iterface Controller '''

    def __init__(self, name):
        self.name = name

    def up(self):
        ''' Brings the interface up. '''
        raise NotImplementedError

    def down(self):
        ''' Brings the interface down. '''
        raise NotImplementedError

    def show(self):
        ''' Shows NIC information. '''
        raise NotImplementedError

    def set_address(self, address):
        ''' Set NIC address. '''
        raise NotImplementedError

    def get_address(self):
        ''' Gets NIC address. '''
        raise NotImplementedError

    def _get_address_getter(self):
        return self.get_address()

    def _set_address_getter(self, address):
        return self.set_address(address)

    address = property(_get_address_getter, _set_address_getter)

    def get_mac(self):
        ''' Gets MAC address. '''
        raise NotImplementedError

    def _get_mac_getter(self):
        return self.get_mac()

    mac = property(_get_mac_getter)

    def get_is_active(self):
        ''' Returns True if NIC is active. '''
        raise NotImplementedError

    def _get_is_active_getter(self):
        return self.get_is_active()

    is_active = property(_get_is_active_getter)

    # multicast is used with ipv6
    def set_multicast(self, m):
        ''' Set multicast for the interface. '''
        raise NotImplementedError

    def get_multicast(self, m):
        ''' Gets multicast for the interface. '''
        raise NotImplementedError

    def _get_multicast_getter(self):
        return self.get_multicast()

    def _set_multicast_getter(self, m):
        return self.set_multicast(self, m)

    multicast = property(_get_multicast_getter, _set_multicast_getter)

    def filtering(*args, **kargs):
        ''' Enables or disables filtering. '''
        raise NotImplementedError

    def __str__(self):
        return '<NIC: %s>' % self.name

    def __repr__(self):
        return '<NIC: %s>' % self.name

class BaseRoute(object):
    ''' Rapresents a route controller. '''

    @staticmethod
    def reset_routes():
        ''' We have no routes '''
        raise NotImplementedError

    @staticmethod
    def default_route(ip, cidr, gateway, dev):
        ''' Maintains this default route for this destination. '''
        # ip and cidr are strings.
        # Eg: '192.168.0.0', '16'
        #
        # gateway and dev are strings.
        # Eg: '192.12.1.1', 'eth1'
        # If gateway is None then there are no routes (UNREACHABLE).
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.
        raise NotImplementedError

    @staticmethod
    def prev_hop_route(ip, cidr, prev_hop, gateway, dev):
        ''' Maintains this route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''
        # ip, cidr and prev_hop are strings. prev_hop is a MAC address.
        # Eg: '192.168.0.0', '16', '6a:b8:1e:cf:1d:4f'
        #
        # gateway and dev are strings.
        # Eg: '192.12.1.1', 'eth1'
        # If gateway is None then there are no routes (UNREACHABLE).
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.
        raise NotImplementedError

    @staticmethod
    def prev_hop_route_default(ip, cidr, prev_hop):
        ''' Use default route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''
        # ip, cidr and prev_hop are strings. prev_hop is a MAC address.
        # Eg: '192.168.0.0', '16', '6a:b8:1e:cf:1d:4f'
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.
        raise NotImplementedError

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Adds a new route with corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def add_neigh(ip, dev=None):
        ''' Adds a new neighbour with corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def change(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Edits the route with the corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def change_neigh(ip, dev=None):
        ''' Edits the neighbour with the corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Removes the route with the corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def delete_neigh(ip, dev=None):
        ''' Removes the neighbour with the corresponding properties. '''
        raise NotImplementedError

    @staticmethod
    def flush():
        ''' Flushes all routes. '''
        raise NotImplementedError

    @staticmethod
    def flush_cache():
        ''' Flushes cache '''
        raise NotImplementedError

    @staticmethod
    def ip_forward(enable):
        ''' Enables/disables ip forwarding. '''
        raise NotImplementedError
