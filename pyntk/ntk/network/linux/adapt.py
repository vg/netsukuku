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


import os
import re
import subprocess

from ntk.config import settings, ImproperlyConfigured
from ntk.network.interfaces import BaseNIC, BaseRoute

def file_write(path, data):
    ''' Writes `data' to the file pointed by `path' '''
    fout = open(path, 'w')
    try:
        fout.write(data)
    finally:
        fout.close()

class IPROUTECommandError(Exception):
    ''' A generic iproute exception '''

def iproute(args):
    ''' An iproute wrapper '''

    try:
        IPROUTE_PATH = settings.IPROUTE_PATH
    except AttributeError:
        IPROUTE_PATH = os.path.join('/', 'sbin', 'ip')

    if not os.path.isfile(IPROUTE_PATH):
        error_msg = ('Can not find %s.\n'
                     'Have you got iproute properly installed?')
        raise ImproperlyConfigured(error_msg % IPROUTE_PATH)

    args_list = args.split()
    cmd = [IPROUTE_PATH] + args_list
    proc = subprocess.Popen(cmd,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()

    if stderr_value:
        raise IPROUTECommandError(stderr_value)

    return stdout_value

class NIC(BaseNIC):
    ''' Network Interface Controller handled using iproute '''

    def up(self):
        ''' Brings the interface up. '''
        iproute('link set %s up' % self.name)

    def down(self):
        ''' Brings the interface down. '''
        iproute('link set %s down' % self.name)

    def show(self):
        ''' Shows NIC information. '''
        return iproute('addr show %s' % self.name)

    def _flush(self):
        ''' Flushes all NIC addresses. '''
        iproute('addr flush dev %s' % self.name)

    def set_address(self, address):
        ''' Set NIC address.
        To remove NIC address simply put it to None or ''.

        @param address: Address of the interface.
        @type address: C{str}
        '''
        # We use only one address for NIC, so a flush is needed.
        if self.address:
            self._flush()
        if address is not None and address != '':
            iproute('addr add %s dev %s' % (address, self.name))

    def get_address(self):
        ''' Gets NIC address. '''
        if settings.IP_VERSION == 4:
            r = re.compile(r'''inet\s((?:\d{1,3}\.){3}\d{1,3})/''')
        matched_address = r.search(self.show())
        return matched_address.groups()[0] if matched_address else None

    def filtering(self, *args, **kargs):
        ''' Enables or disables filtering. '''

        self._rp_filter(*args, **kargs)

    def _rp_filter(self, enable=True):
        ''' Enables or disables rp filtering. '''
        path = '/proc/sys/net/ipv%s' % settings.IP_VERSION
        path = os.path.join(path, 'conf', self.name, 'rp_filter')

        value = '1' if enable else '0'

        file_write(path, value)


    def __str__(self):
        return 'NIC: %s' % self.name

class Route(BaseRoute):
    ''' Managing routes using iproute
    '''

    ##TODO: add the possibility to specify a routing table, f.e. 'table ntk'

    @staticmethod
    def _add_delete_cmd(command, ip, cidr, dev, gateway):
        ''' Returns proper iproute command arguments to add and delete routes
        '''
        cmd = 'route %s %s/%s' % (command, ip, cidr)

        if dev is not None:
            cmd += ' dev %s' % dev
        if gateway is not None and gateway != ip:
            cmd += ' via %s' % gateway

        cmd += ' protocol ntk'
#        cmd += ' table ntk'

        return cmd

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None):
        ''' Adds a new route with corresponding properties. '''
        cmd = Route._add_delete_cmd('add', ip, cidr, dev, gateway)
        iproute(cmd)

    @staticmethod
    def change(properties):
        pass

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None):
        ''' Removes a route with corresponding properties. '''
        cmd = Route._add_delete_cmd('del', ip, cidr, dev, gateway)
        iproute(cmd)

    @staticmethod
    def flush():
        ''' Flushes the routing table '''
        iproute('route flush')

    @staticmethod
    def flush_cache():
        ''' Flushes cache '''
        iproute('route flush cache')

    @staticmethod
    def ip_forward(enable=True):
        ''' Enables/disables ip forwarding. '''
        path = '/proc/sys/net/ipv%s' % settings.IP_VERSION

        if settings.IP_VERSION == 4:
            path = os.path.join(path, 'ip_forward')
        elif settings.IP_VERSION == 6:
            # Enable forwarding for all interfaces
            path = os.path.join(path, 'conf/all/forwarding')

        value = '1' if enable else '0'

        file_write(path, value)
