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
# Coordinator Node
#
# TODO: tmp_deleted_purge() isn't called, moreover a fixed timeout isn't a
#       good thing, you have to consider the rtt from the requester node to
#       the coordinator node.


from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc
from ntk.wrap.xtime import time
from ntk.core.p2p import P2P
from ntk.lib.rencode import serializable
from ntk.core.map import Map
from random import choice
from ntk.network.inet import valid_ids


class Node(object):

    def __init__(self, lvl, id, alive=False, its_me=False):
        self.lvl = lvl
        self.id = id
        self.its_me = its_me
        self.alive = alive

    def is_free(self):
        if self.its_me: return False
        return not self.alive

    def _pack(self):
        # lvl and id are not used (as for now) at the time of de-serialization. So
        # use the value that will produce the smaller output with rencode.dumps.
        # TODO test what this value is... perhaps None is better than 0 ?
        return (0, 0, self.alive)

serializable.register(Node)

class MapCache(Map):
    def __init__(self, maproute):
        logging.log(logging.ULTRADEBUG, 'Coord: copying a mapcache from our maproute.')
        Map.__init__(self, maproute.levels, maproute.gsize, Node, maproute.me)

        self.copy_from_maproute(maproute)
        self.remotable_funcs = [self.map_data_merge]

        self.tmp_deleted = {}

    def copy_from_maproute(self, maproute):
        for lvl in xrange(self.levels):
                for id in xrange(self.gsize):
                        if not maproute.node_get(lvl, id).is_empty():
                                self.deriv_node_add(lvl, id)

    def deriv_node_add(self, lvl, id):
        if self.node_get(lvl, id).is_free():
                self.node_get(lvl, id).alive = True
                self.node_add(lvl, id)

    def tmp_deleted_add(self, lvl, id):
        self.tmp_deleted[lvl, id] = time()
        self.node_del(lvl, id, silent=1)

    def tmp_deleted_del(self, lvl, id):
        """Removes an entry from the self.tmp_deleted cache"""
        if (lvl, id) in self.tmp_deleted:
            del self.tmp_deleted[lvl, id]

    @microfunc()
    def tmp_deleted_purge(self, timeout=32):
        """After a `timeout' seconds, restore a node added in the tmp_deleted
           cache"""

        new_tmp_deleted = {}

        curt = time()
        for lvl, id in self.tmp_deleted:
            t = self.tmp_deleted[lvl, id]
            if curt-t >= timeout:
                self.node_add(lvl, id, silent=1)
            else:
                new_tmp_deleted[lvl, id] = t

        self.tmp_deleted = new_tmp_deleted


class Coord(P2P):

    pid = 1

    def __init__(self, ntkd, radar, maproute, p2pall):

        P2P.__init__(self, radar, maproute, Coord.pid)
        self.ntkd = ntkd

        # let's register ourself in p2pall
        p2pall.p2p_register(self)

        # The cache of the coordinator node
        self.mapcache = MapCache(self.maproute)

        self.maproute.events.listen('NODE_NEW', self.mapcache.deriv_node_add)
        self.maproute.events.listen('NODE_DELETED', self.mapcache.node_del)

        self.mapp2p.events.listen('NODE_NEW', self.new_participant_joined)

        self.coordnode = [None] * (self.maproute.levels + 1)

        self.remotable_funcs += [self.going_out, self.going_out_ok, self.going_in]


    def h(self, key):
        """h:KEY-->IP
        :type key: a tuple (lvl, ip)
        """
        lvl, ip = key
        IP = list(ip)
        for l in reversed(xrange(lvl)):
            IP[l] = 0
        return IP

    def coord_nodes_set(self):
        """Sets the coordinator nodes of each level, using the current map"""
        logging.log(logging.ULTRADEBUG, 'Coord: calculating coord_nodes for our gnodes of each level...')
        for lvl in xrange(self.maproute.levels):
                self.coordnode[lvl+1] = self.H(self.h((lvl+1, self.maproute.me)))
        logging.log(logging.ULTRADEBUG, 'Coord: coord_nodes (Note: check from the second one) is now ' + str(self.coordnode))

    def participate(self):
        """Let's become a participant node"""
        P2P.participate(self)  # base method
        self.coord_nodes_set()


    @microfunc()
    def new_participant_joined(self, lvl, id):
        """Shall the new participant succeed us as a coordinator node?"""

        logging.log(logging.ULTRADEBUG, 'Coord: new_participant_joined started, a new participant in level ' + str(lvl) + ' id ' + str(id))
        # the node joined in level `lvl', thus it may be a coordinator of the
        # level `lvl+1'
        level = lvl + 1

        # The new participant has this part of NIP
        pIP = self.maproute.me[:]

        pIP[lvl] = id

        for l in reversed(xrange(lvl)): pIP[l] = None
        # Note: I don't know its exact IP, it may have some None in lower-than-lvl levels.

        # Was I the previous coordinator? Remember it.
        it_was_me = self.coordnode[level] == self.maproute.me

        # perfect IP for this service is...
        hIP = self.h((level, self.maproute.me))
        # as to our knowledge, the nearest participant to 'hIP' is now...
        HhIP = self.H(hIP)
        # Is it the new participant?
        for j in xrange(lvl, self.maproute.levels):
            if HhIP[j] != pIP[j]:
                # the new participant isn't a coordinator node
                return

        # Yes it is. Keep track.
        self.coordnode[level] = HhIP
        logging.info('Coord: new coordinator for our level ' + str(level) + ' is ' + str(HhIP))

        # Then, if I was the previous one... (Tricky enough, new participant could just be me!)
        if it_was_me and HhIP != self.maproute.me:
            # ... let's pass it our cache
            logging.debug('Coord: I was coordinator for our level ' + str(level) + ', new coordinator is ' + str(HhIP))
            logging.debug('Coord: So I will pass him my mapcache.')
            peer = self.peer(hIP=hIP)
            def fmake_alive(node):
                node.alive = True
            peer.mapcache.map_data_merge(self.mapcache.map_data_pack(fmake_alive))
            logging.debug('Coord: Done passing my mapcache.')


    def going_out(self, lvl, id, gnumb=None):
        """The node of level `lvl' and ID `id', wants to go out from its gnode

           G of level lvl+1. We are the coordinator of this gnode G.
           We'll give an affermative answer if `gnumb' < |G| or if
           `gnumb'=None"""

        
        if (gnumb < self.mapcache.nodes_nb[lvl]-1 or gnumb is None)       \
                and self.mapcache.node_get(lvl, id).alive:
                self.mapcache.node_del(lvl, id)
                return self.mapcache.nodes_nb[lvl]
        else:
            return None

    def going_out_ok(self, lvl, id):
        """The node, which was going out, is now acknowledging the correct
        migration"""
        self.mapcache.tmp_deleted_del(lvl, id)

    def going_in(self, lvl, gnumb=None):
        """A node wants to become a member of our gnode G of level `lvl+1'.

           We are the coordinator of this gnode G (so we are also a member of G).
           We'll give an affermative answer if `gnumb' > |G| or if
           `gnumb'=None"""

        logging.log(logging.ULTRADEBUG, 'Coord.going_in: The requested level is ' + str(lvl))
        logging.log(logging.ULTRADEBUG, 'Coord.going_in: This is mapcache.')
        logging.log(logging.ULTRADEBUG, self.mapcache.repr_me())

        if gnumb and not gnumb > self.mapcache.nodes_nb[lvl]+1: return None

        fnl = self.mapcache.free_nodes_list(lvl)
        if fnl == []:
                return None

        newnip = self.mapcache.me
        newnip[lvl] = choice(fnl)
        for l in reversed(xrange(lvl)): newnip[l] = choice(valid_ids(lvl, newnip))
        # it should be previously free...
        node = self.mapcache.node_get(lvl, newnip[lvl])
        if node.is_free():
            node.alive = True
            self.mapcache.node_add(lvl, newnip[lvl])
        return newnip

