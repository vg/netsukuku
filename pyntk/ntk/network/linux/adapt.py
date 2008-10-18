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

import os
import re
import subprocess

from ntk.network.interfaces import BaseNIC, BaseRoute


class IPROUTECommandError(Exception):
    ''' A generic iproute exception '''

IPROUTE_PATH = os.path.join('/','bin', 'ip')

def iproute(args):
    ''' An iproute wrapper '''
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
        # TODO: use global settings to determine ip version
        #       following implementation is for ipv4 only
        r = re.compile(r'''inet\s((?:\d{1,3}\.){3}\d{1,3})/''')
        matched_address = r.search(self.show())
        return matched_address.groups()[0] if matched_address else None

    def __str__(self):
        return 'NIC: %s' % self.name

class Route(BaseRoute):
    '''  '''