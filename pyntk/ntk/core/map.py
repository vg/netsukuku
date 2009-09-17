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

from ntk.lib.log import logger as logging
from random import randint, choice
from ntk.lib.log import get_stackframes

from ntk.lib.event import Event
from ntk.network.inet import valid_ids
import ntk.wrap.xtime as xtime


class DataClass(object):
    """Data class example.

    A Data class contains information regarding a node of the map.
    Each Map.node[level][id] entry is a Data class instance.

    This Data class is just a stub.
    As another example, look MapRoute and RouteNode in route.py
    """

    def __init__(self, level, id, its_me=False):
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
        if me is None: self.me = None
        else: self.me = me[:]     # Ourself. self.me[lvl] is the ID of our
                                  # (g)node of level lvl
        # Choose a random nip
        if me is None:
            self.me = self.nip_rand()

        # The member self.node[l][i] is a node of level l and its ID is i
        self.node = [[None] * self.gsize for i in xrange(self.levels)]
        # Number of nodes of each level, that is:
        #   self.node_nb[i] = number of (g)nodes inside the gnode self.me[i+1]
        self.node_nb = [0] * self.levels

        for lvl in xrange(self.levels):
            node_me = self.node_get(lvl, self.me[lvl])
            if not node_me.is_free(): self.node_add(lvl, self.me[lvl], silent=1)

        self.events = Event(['NODE_NEW', 'NODE_DELETED', 'ME_CHANGED'])

    def node_get(self, lvl, id):
        """Returns from the map a node of level `lvl' and id `id'.

        An instance of type `self.dataclass' will always be returned: if
        it doesn't exist, it is created"""

        if self.node[lvl][id] is None:
            if self.me is not None and self.me[lvl] == id:
                self.node[lvl][id] = self.dataclass(lvl, id, its_me=True)
            else:
                self.node[lvl][id] = self.dataclass(lvl, id)
        return self.node[lvl][id]

    def node_add(self, lvl, id, silent=0):
        """Add node 'id` at level 'lvl'.
        
        The caller of this method has the responsibility to check that the node was
        previously free, and that now it is busy. This method just sends the event
        and updates the counters"""
        node = self.node[lvl][id]
        if node is not None and not node.is_free():
            self.node_nb[lvl] += 1
            if not silent:
                self.events.send('NODE_NEW', (lvl, id))

    def node_del(self, lvl, id, silent=0):
        """Delete node 'id` at level 'lvl'.
        
        This method checks that the node was previously busy. Then it deletes the
        node, sends the event and updates the counters"""
        node = self.node[lvl][id]
        if node is not None and not node.is_free():
            self.node[lvl][id]=None
            if self.node_nb[lvl] > 0:
                self.node_nb[lvl] -= 1
            if not silent:
                self.events.send('NODE_DELETED', (lvl, id))

    def free_nodes_nb(self, lvl):
        """Returns the number of free nodes of level `lvl'"""
        #it depends on the lvl and on the previous ids
        return len(valid_ids(lvl, self.me))-self.node_nb[lvl]

    def free_nodes_list(self, lvl):
        """Returns the list of free nodes of level `lvl'"""

        #it depends on the lvl and on the previous ids
        return [nid for nid in valid_ids(lvl, self.me) if (not self.node[lvl][nid]) or self.node[lvl][nid].is_free()]

    def is_in_level(self, nip, lvl):
        """Does the node nip belongs to our gnode of level `lvl'?"""
        return nip[:-lvl-1] == self.me[:-lvl-1]

    def lvlid_to_nip(self, lvl, id):
        """Converts a (lvl, id) pair, referring to this map, to
           its equivalent netsukuku ip"""
        nip = self.me[:]
        nip[lvl] = id
        for l in reversed(xrange(lvl)):
            nip[l] = 0
        return nip

    def ip_to_nip(self, ip):
        """Converts the given ip to a nip (Netsukuku IP)

        A nip is a list [a_0, a_1, ..., a_{n-1}], where n = self.levels
        and such that a_{n-1}*g^{n-1}+a_{n-2}*g^(n-2)+...+a_0 = ip,
        where g = self.gsize"""

        g = self.gsize
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
        nip = [0 for i in xrange(self.levels)]
        for lvl in reversed(xrange(self.levels)):
            nip[lvl] = self._nip_rand(lvl, nip)
        return nip

    def _nip_rand(self, lvl, nip):
        """Returns a random id for level lvl that is valid, given that the previous ids are in nip"""
        return choice(valid_ids(lvl, nip))

    def level_reset(self, level):
        """Resets the specified level, without raising any event"""

        self.node[level] = [None] * self.gsize

        self.node_nb[level] = 0
        node_me = self.node_get(level, self.me[level])
        if not node_me.is_free(): self.node_add(level, self.me[level], silent=1)

    def map_reset(self):
        """Silently resets the whole map"""
        for l in xrange(self.levels):
            self.level_reset(l)

    def me_change(self, new_me, silent=False):
        """Changes self.me"""

        # changing my nip will make many nodes no more significant in my map
        lev = self.nip_cmp(self.me, new_me)
        if lev == -1: return  # the same old nip
        for l in xrange(lev):
            self.level_reset(l)
        # silently remove the dataclass objects representing old me (current)
        for l in xrange(self.levels):
                self.node[l][self.me[l]] = None
        # now, change

        old_me = self.me[:]
        self.me = new_me[:]
        # silently add the dataclass objects representing new me
        for l in xrange(self.levels):
                self.node[l][self.me[l]] = self.dataclass(l, self.me[l], its_me=True)
        if not silent:
                self.events.send('ME_CHANGED', (old_me, self.me))

    def map_data_pack(self, func=None):
        """Prepares a packed_map to be passed to map_data_merge in another host.
        
        "func" is a function that receives a node and makes it not free.
        It is needed to make a node "normally busy" that is "its_me busy"
        in my point of view, but won't be in the other nip's point of view."""
        ret = (self.me, [ [self.node[lvl][id] for id in xrange(self.gsize)]
                             for lvl in xrange(self.levels) ],
                [self.node_nb[lvl] for lvl in xrange(self.levels)])
        for lvl in xrange(self.levels):
            # self.me MUST be replaced
            # with a normal node
            node = self.dataclass(lvl, self.me[lvl])
            if func: func(node)
            ret[1][lvl][self.me[lvl]] = node
        # It's been a tough work! And now we'll probably serialize the result!
        # Be kind to other tasks.
        xtime.swait(10)
        return ret

    def map_data_merge(self, (nip, plist, nblist)):
        """Copies a map from another nip's point of view."""
        lvl=self.nip_cmp(nip, self.me)

        for l in xrange(lvl, self.levels):
                # It's a tough work! Be kind to other tasks.
                xtime.swait(10)
                self.node_nb[l]=nblist[l]
                for id in xrange(self.gsize):
                    if id != self.me[l]:  # self.me MUST NOT be replaced
                                          # with a normal node
                        self.node[l][id]=plist[l][id]

        for l in xrange(0, lvl):
                self.level_reset(l)

    def repr_me(self, func_repr_node=None):
        ret = 'me ' + str(self.me) + ', node_nb ' + str(self.node_nb) + ', {'
        for lvl in xrange(self.levels):
            ret += self.repr_level(lvl, func_repr_node)
        ret += '}'
        return ret

    def repr_level(self, lvl, func_repr_node=None):
        def repr_node_map(node):
            if node.is_free(): return ' '
            return 'X'
        if func_repr_node is None: func_repr_node = repr_node_map
        ret = ' ['
        for i in xrange(self.gsize):
            ret += '\'' + func_repr_node(self.node_get(lvl, i))
        ret += '\'] '
        return ret


