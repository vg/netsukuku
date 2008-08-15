##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
#
# Implementation of the map. See {-topodoc-}
# 

from ntk.lib.event import Event
from random import randint

class DataClass:
    """Data class example.

    A Data class contains information regarding a node of the map.
    Each Map.node[level][id] entry is a Data class instance.

    This Data class is just a stub.
    As another example, look MapRoute and RouteNode in route.py
    """

    def __init__(self, level, id):
        # do something
        pass

    def is_free(self):
        """Returns True if this data class is free. False otherwise"""
        return True

class Map(object):

    __slots__ = ['levels', 'gsize', 'dataclass', 'me', 'node', 'node_nb',
                 'events']

    def __init__(self, levels, gsize, dataclass, me=None):
        """Initialise the map

        If me = None, then self.me is set to a random nip (ntk ip)
        """

        self.levels = levels   # Number of levels
        self.gsize = gsize     # How many nodes are contained in a gnode
        self.dataclass = dataclass
        self.me = me        # Ourself. self.me[lvl] is the ID of our
                            # (g)node of level lvl
        # Choose a random nip
        if me is None:
            self.me = self.nip_rand()

        # The member self.node[l][i] is a node of level l and its ID is i
        self.node = [[None] * self.gsize] * self.levels
        # Number of nodes of each level
        self.node_nb = [0] * self.levels 

        self.events = Event( [ 'NODE_NEW', 'NODE_DELETED', 'ME_CHANGED' ] )

    def node_get(self, lvl, id):
        """Returns from the map a node of level `lvl' and id `id'.

        An instance of type `self.dataclass' will always be returned: if
        it doesn't exist, it is created"""

        if self.node[lvl][id] is None:
            self.node[lvl][id] = self.dataclass(lvl, id)
        return self.node[lvl][id]

    def node_add(self, lvl, id, silent=0):
        self.node_get(lvl, id)
        self.node_nb[lvl] += 1
        if not silent:
            self.events.send('NODE_NEW', (lvl, id))

    def node_del(self, lvl, id, silent=0):
        ''' Delete node 'id` at level 'lvl` '''
        if self.node_nb[lvl] > 0:
            self.node_nb[lvl] -= 1

        if not silent:
            self.events.send('NODE_DELETED', (lvl, id))
        self.node[lvl][id]=None

    def free_nodes_nb(self, lvl):
        """Returns the number of free nodes of level `lvl'"""
        return self.gsize-self.node_nb[lvl]

    def free_nodes_list(self, lvl):
        """Returns the list of free nodes of level `lvl'"""
        return [nid for n in self.node[lvl] 
                        for nid in xrange(self.gsize)
                            if self.node[lvl][nid].is_free() ]

    def is_in_level(self, nip, lvl):
        """Does the node nip belongs to our gnode of level `lvl'?"""
        return nip[:-lvl-1] == self.me[:-lvl-1]

    def lvlid_to_nip(self, lvl, id):
        """Converts a (lvl, id) pair, referring to this map, to 
           its equivalent netsukuku ip"""
        nip=self.me[:]
        nip[lvl]=id
        for l in reversed(xrange(lvl)): nip[l]=None
        return nip

    def ip_to_nip(self, ip):
        """Converts the given ip to a nip (Netsukuku IP)

        A nip is a list [a_0, a_1, ..., a_{n-1}], where n = self.levels
        and such that a_{n-1}*g^{n-1}+a_{n-2}*g^(n-2)+...+a_0 = ip, 
        where g = self.gsize"""

        g=self.gsize
        return [(ip % g**(l+1)) / g**l for l in xrange(self.levels)]

    def nip_to_ip(self, nip):
        """The reverse of ip_to_nip"""

        g=self.gsize
        return sum([nip[l] * g**l for l in xrange(self.levels)])

    def nip_cmp(self, nipA, nipB):
        """Returns the first level where nipA and nipB differs. The search
        start from the end of the nip """

        for lvl in reversed(xrange(self.levels)):
            if nipA[lvl] != nipB[lvl]:
                return lvl

        return -1

    def nip_rand(self):
        """Returns a random netsukuku ip"""
        return [randint(0, self.gsize-1) for i in xrange(self.levels)]

    def level_reset(self, level):
        """Resets the specified level, without raising any event"""
        self.node[level]    = [None]*self.gsize
        self.node_nb[level] = 0

    def map_reset(self):
        """Silently resets the whole map"""
        for l in xrange(self.levels):
                self.level_reset(l)

    def me_change(self, new_me):
        """Changes self.me"""
        old_me=self.me[:]
        self.me=new_me
        self.events.send('ME_CHANGED', (old_me, new_me))


    def map_data_pack(self):
        return (self.me, [ [self.node[lvl][id] for id in xrange(self.gsize)]
                             for lvl in xrange(self.levels) ],
                [self.node_nb[lvl] for lvl in xrange(self.levels)])

    def map_data_merge(self, (nip, plist, nblist)):
        lvl=self.nip_cmp(nip, self.me)
        for l in xrange(lvl, self.levels):
                self.node_nb[l]=nblist[l]
                for id in xrange(self.gsize):
                        self.node[l][id]=plist[l][id]
        for l in xrange(0, lvl):
                self.level_reset(l)
