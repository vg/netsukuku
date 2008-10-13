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
import subprocess

from ntk.network.interfaces import BaseNIC, BaseRoute


class IPCommandError(Exception):
    pass

IP_CMD = os.path.join('/','bin', 'ip')

def ip_cmd(args):
    args_list = args.split()
    cmd = [IP_CMD] + args_list
    proc = subprocess.Popen(cmd,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()

    if stderr_value:
        raise IPCommandError(stderr_value)

    return stdout_value

class NIC(BaseNIC):
    ''' Rapresents  '''

    def up(self):
        ip_cmd('link set %s up' % self.name)

    def down(self):
        ip_cmd('link set %s down' % self.name)

    def show(self):
        print ip_cmd('addr show %s' % self.name)

    def set_address(self, address):
        ip_cmd('addr flush dev %s' % self.name)
        ip_cmd('addr add %s dev %s' % (address, self.name))

    def get_address(self):
        return None

    def __str__(self):
        return 'NIC: %s' % self.name

class Route(BaseRoute):
    '''  '''