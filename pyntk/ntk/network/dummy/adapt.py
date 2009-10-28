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


from ntk.network.interfaces import BaseNIC, BaseRoute

class NIC(BaseNIC):
    ''' Dummy Network Interface Controller '''

    def up(self):
        pass

    def down(self):
        pass

    def show(self):
        print 'Netsukuku Dummy Interface'

    def set_address(self, address):
        pass

    def get_address(self):
        return None

    def get_mac(self):
        return None

    def filtering(self, *args, **kargs):
        pass

class Route(BaseRoute):
    ''' Dummy route  '''

    @staticmethod
    def default_route(ip, cidr, gateway, dev):
        ''' Maintains this default route for this destination. '''

    @staticmethod
    def prev_hop_route(ip, cidr, prev_hop, gateway, dev):
        ''' Maintains this route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''

    @staticmethod
    def prev_hop_route_default(ip, cidr, prev_hop):
        ''' Use default route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Adds a new route with corresponding properties. '''

    @staticmethod
    def add_neigh(ip, dev=None):
        ''' Adds a new neighbour with corresponding properties. '''

    @staticmethod
    def change(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Edits the route with the corresponding properties. '''

    @staticmethod
    def change_neigh(ip, dev=None):
        ''' Edits the neighbour with the corresponding properties. '''

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Removes the route with the corresponding properties. '''

    @staticmethod
    def delete_neigh(ip, dev=None):
        ''' Removes the neighbour with the corresponding properties. '''

    @staticmethod
    def flush():
        ''' Flushes all routes. '''

    @staticmethod
    def flush_cache():
        ''' Flushes cache '''

    @staticmethod
    def ip_forward(enable):
        ''' Enables/disables ip forwarding. '''
