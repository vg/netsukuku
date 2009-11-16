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


from ntk.network.interfaces import BaseNIC, BaseRoute

class NIC(BaseNIC):
    ''' Dummy Network Interface Controller '''

    def up(self):
        '''Brings the interface up.'''

    def down(self):
        '''Brings the interface down.'''

    def show(self):
        '''Shows NIC information.'''
        print 'Netsukuku Dummy Interface'

    def flush(self):
        '''Flushes all NIC addresses.'''

    def set_address(self, address):
        '''Set NIC address.'''

    def get_address(self):
        '''Gets NIC address.'''

    def get_mac(self):
        '''Gets NIC MAC address'''

    def filtering(self, *args, **kargs):
        '''Enables or disables filtering.'''

class Route(BaseRoute):
    '''Dummy route'''

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None):
        '''Adds a new route with corresponding properties.'''

    @staticmethod
    def change(properties):
        '''Edits the route with the corresponding properties.'''

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None):
        '''Removes the route with the corresponding properties.'''

    @staticmethod
    def flush():
        '''Flushes all routes.'''

    @staticmethod
    def flush_cache():
        '''Flushes cache'''

    @staticmethod
    def ip_forward(enable):
        '''Enables/disables ip forwarding.'''
