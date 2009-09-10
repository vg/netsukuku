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


import sys

if sys.platform == 'linux2':
    NETWORK_BACKEND = 'linux'
else:
    NETWORK_BACKEND = 'dummy'

backend = __import__('ntk.network.%s.adapt' % NETWORK_BACKEND, {}, {}, [''])

NIC = backend.NIC
Route = backend.Route


class NICError(Exception):
    ''' Generic NIC error '''

class NICDoesNotExist(NICError):
    ''' NIC does not exist '''

class MultipleNICReturned(NICError):
    ''' Multiple NIC returned instead of one '''


class NICManager(object):
    ''' A NIC manager to handle all node's nics '''

    def __init__(self, nics=None, exclude_nics=None):

        if nics is None:
            raise NICError('No NIC specified!')

        if exclude_nics is None:
            exclude_nics = []

        self._nics = dict([(n, NIC(n)) for n in nics if n not in exclude_nics])

    def __getitem__(self, key):
        return self._nics[key]

    def __iter__(self):
        return iter(self._nics)

    def all(self):
        ''' Returns all managed NICs. '''
        return self._nics.values()

    def filter(self, **kwargs):
        ''' Returns a list of NICs matching the given keyword arguments. '''

        def _check(nic, **kwargs):
            r = []
            for arg in kwargs:
                value = getattr(nic, arg)
                r.append(value == kwargs[arg])
            return all(r)

        return [n for n in self._nics.values() if _check(n, **kwargs)]

    def get(self, **kwargs):
        ''' Returns a single NIC matching the given keyword arguments. '''
        result = self.filter(**kwargs)

        if not result:
            raise NICDoesNotExist('NIC does not exist!')

        count = len(result)
        if count == 1:
            return result[0]
        else:
            raise MultipleNICReturned('Returned %s instead of one NIC' % count)

    def up(self):
        ''' Brings all interfaces up '''
        for n in self._nics:
            self._nics[n].up()

    def down(self):
        ''' Brings all interfaces down '''
        for n in self._nics:
            self._nics[n].down()

    def activate(self, address):
        ''' Set same `address' for every NIC of the node

        Every ntk node is unique in the network, so we assign the same IP
        address. TODO: assess ARP problems' probability
        '''
        for n in self._nics:
            self._nics[n].down()
            self._nics[n].up()
            self._nics[n].address = address
