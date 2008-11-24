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


import sys

if sys.platform == 'linux2':
    NETWORK_BACKEND = 'linux'
else:
    NETWORK_BACKEND = 'dummy'

backend = __import__('ntk.network.%s.adapt' % NETWORK_BACKEND, {}, {}, [''])

NIC = backend.NIC
Route = backend.Route

class NICManager(object):
    ''' A NIC manager to handle all node's nics '''

    def __init__(self, nics=None, exclude_nics=None):

        if nics is None:
            raise Exception('No NIC specified!')
        if exclude_nics is None:
            exclude_nics = []

        self._nics = dict([(n, NIC(n)) for n in nics if n not in exclude_nics])

    def __getitem__(self, key):
        return self._nics[key]

    def __iter__(self):
        return iter(self._nics)

    def up(self):
        ''' Brings all interfaces up '''
        for n in self._nics:
            n.up()

    def down(self):
        ''' Brings all interfaces down '''
        for n in self._nics:
            n.down()
