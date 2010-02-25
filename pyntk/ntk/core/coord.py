##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# (c) Copyright 2009 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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

from random import choice

from ntk.core.map import Map
from ntk.core.p2p import P2P
from ntk.lib.log import logger as logging
from ntk.lib.log import get_stackframes
from ntk.lib.micro import microfunc
from ntk.lib.rencode import serializable
from ntk.network.inet import valid_ids
from ntk.wrap.xtime import time, swait
from ntk.core.status import ZombieException


class Node(object):

    def __init__(self, the_map, lvl, id, alive=False, its_me=False):
        self.lvl = lvl
        self.id = id
        self.its_me = its_me
        self.alive = alive
        if self.its_me:
            self.alive = True

    def is_free(self):
        return not self.alive

    def _pack(self):
        # lvl and id are not used (as for now) at the time of 
        # de-serialization. Nor it is the_map.
        # So use the value that will produce the 
        # smaller output with rencode.dumps.
        # TODO test what this value is... perhaps None is better than 0 ?
        return (0, 0, 0, self.alive)

serializable.register(Node)

class MapCache(Map):
    def __init__(self, ntkd_status, maproute):
        logging.log(logging.ULTRADEBUG, 'Coord: copying a mapcache from our '
                                        'maproute.')
        Map.__init__(self, maproute.levels, maproute.gsize, Node, maproute.me)

        self.ntkd_status = ntkd_status
        self.copy_from_maproute(maproute)
        self.remotable_funcs = [self.map_data_merge]

        self.tmp_deleted = {}

    def alive_node_add(self, lvl, id):
        # It is called:
        #  * by copy_from_maproute, when I begin to participate to this 
        #    service (usually at boot when we have a temporary NIP!).
        #  * by MapRoute's event NODE_NEW, when I come to know a new 
        #    destination.
        #  * by going_in, if I *am* the coordinator, when I accept a request.
        if self.node_get(lvl, id).is_free():
            self.node_get(lvl, id).alive = True
            self.node_add(lvl, id)
        logging.log(logging.ULTRADEBUG, 'Coord: MapCache updated: ' + 
                    str(self.repr_me()))

    def me_changed(self, old_me, new_me):
        '''Changes self.me

        :param old_me: my old nip (not used in MapCache)
        :param new_me: new nip
        '''
        Map.me_change(self, new_me)
        logging.log(logging.ULTRADEBUG, 'Coord: MapCache updated after '
        '                                me_changed: ' + str(self.repr_me()))

    def copy_from_maproute(self, maproute):
        for lvl in xrange(self.levels):
            for id in xrange(self.gsize):
                if not maproute.node_get(lvl, id).is_free():
                    self.alive_node_add(lvl, id)

    def map_data_pack(self):
        """Prepares a packed_mapcache to be passed to mapcache.map_data_merge
        in another host."""
        def fmake_alive(node):
            node.alive = True
        return Map.map_data_pack(self, fmake_alive)

    def map_data_merge(self, (nip, plist, nblist)):
        """Copies a mapcache from another nip's point of view."""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        logging.log(logging.ULTRADEBUG, 'Merging a mapcache.map_data_merge: '
                                        'before: ' + self.repr_me())
        # Was I alive?
        # TODO always alive?
        me_was = [False] * self.levels
        for lvl in xrange(self.levels):
            me_was[lvl] = self.node_get(lvl, self.me[lvl]).alive
        logging.debug('MapCache replication: me_was : ' + str(me_was))
        # Merge as usual...
        lvl=self.nip_cmp(nip, self.me)
        logging.log(logging.ULTRADEBUG, 'Merging a mapcache at level ' + 
                    str(lvl))
        logging.log(logging.ULTRADEBUG, get_stackframes(back=1))
        Map.map_data_merge(self, (nip, plist, nblist))
        # ... ripristine myself.
        for lvl in xrange(self.levels):
            if me_was[lvl]:
                self.alive_node_add(lvl, self.me[lvl])
        logging.log(logging.ULTRADEBUG, 'Merging a mapcache.map_data_merge: '
                                        'after: ' + self.repr_me())

    def repr_me(self, func_repr_node=None):
        def repr_node_mapcache(node):
            if node.alive: return 'X'
            return ' '
        if func_repr_node is None: func_repr_node = repr_node_mapcache
        return Map.repr_me(self, func_repr_node)

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

    def __init__(self, ntkd_status, radar, maproute, p2pall):

        P2P.__init__(self, ntkd_status, radar, maproute, Coord.pid)

        # let's register ourself in p2pall
        p2pall.p2p_register(self)
        p2pall.events.listen('P2P_HOOKED', self.coord_hook)

        # The cache of the coordinator node
        self.mapcache = MapCache(ntkd_status, self.maproute)

        self.maproute.events.listen('NODE_NEW', self.mapcache.alive_node_add)
        self.maproute.events.listen('NODE_DELETED', self.mapcache.node_del)
        self.maproute.events.listen('ME_CHANGED', self.mapcache.me_changed)

        self.maproute.events.listen('NODE_NEW', self.new_participant_joined)
        

        self.coordnode = [None] * (self.maproute.levels + 1)

        self.remotable_funcs += [self.going_out, self.going_out_ok, 
                                 self.going_in]

    def h(self, key):
        """h:KEY-->hIP
        :type key: a tuple (lvl, ip)
        """
        lvl, ip = key
        hIP = list(ip)
        for l in reversed(xrange(lvl)):
            hIP[l] = 0
        return hIP

    def coord_nodes_set(self):
        """Sets the coordinator nodes of each level, 
        using the current map"""
        logging.log(logging.ULTRADEBUG, 'Coord: calculating coord_nodes for '
                                        'our gnodes of each level...')
        for lvl in xrange(self.maproute.levels):
                self.coordnode[lvl+1] = self.search_participant_as_nip(self.h((lvl+1, self.maproute.me)))
        logging.log(logging.ULTRADEBUG, 'Coord: coord_nodes (Note: check from '
                                        'the second one) is now ' + 
                                        str(self.coordnode))

    def coord_hook(self, *args):
        self.coord_nodes_set()

    @microfunc(keep_track=1)
    def new_participant_joined(self, lvl, id):
        """Shall the new participant succeed us as a coordinator node?"""

        logging.log(logging.ULTRADEBUG, 'Coord: new_participant_joined '
                                        'started, a new participant in level ' 
                                        + str(lvl) + ' id ' + str(id))

        # the node joined in level `lvl', thus it may be a coordinator of the
        # level `lvl+1'
        level = lvl + 1

        # The new participant has this part of NIP
        pIP = self.maproute.me[:]

        pIP[lvl] = id

        for l in reversed(xrange(lvl)): pIP[l] = None
        # Note: I don't know its exact IP, it may have some None in 
        # lower-than-lvl levels.

        # Was I the previous coordinator? Remember it.
        it_was_me = self.coordnode[level] == self.maproute.me

        # perfect IP for this service is...
        hIP = self.h((level, self.maproute.me))
        # as to our knowledge, the nearest participant to 'hIP' is now...
        HhIP = self.search_participant_as_nip(hIP)
        # Is it the new participant?
        for j in xrange(lvl, self.maproute.levels):
            if HhIP[j] != pIP[j]:
                # the new participant isn't a coordinator node
                return

        # Yes it is. Keep track.
        self.coordnode[level] = HhIP
        logging.info('Coord: new coordinator for our level ' + str(level) + 
                     ' is ' + str(HhIP))
        
        # Then, if I was the previous one... (Tricky enough, new participant 
        # could just be me!)
        if it_was_me and HhIP != self.maproute.me:
            # ... let's pass it our cache
            logging.debug('Coord: I was coordinator for our level ' + 
                          str(level) + ', new coordinator is ' + str(HhIP))
            logging.debug('Coord: So I will pass him my mapcache.')
            for i in xrange(10):
                try:
                    logging.debug('Coord: prepare to pass to ' + str(hIP))
                    peer = self.peer(hIP=hIP)
                    logging.debug('Coord: prepare map')
                    themap = self.mapcache.map_data_pack()
                    logging.debug('Coord: try to pass to ' + str(hIP))
                    peer.mapcache.map_data_merge(themap)
                    logging.debug('Coord: well done.')
                    break
                except Exception, e:
                    # An exception "no route to host" could be raised because
                    # the route to the new node is not ready yet.
                    # An exception "p2p is hooking" could be raised because
                    # the P2PAll.p2p_hook has not terminated yet.
                    # In any case, we must retry at least for a while.
                    logging.debug('Coord: fail ' + str(e))
                    swait(2000)
            logging.debug('Coord: Done passing my mapcache.')


    def going_out(self, lvl, id, gfree_new=None):
        """The node of level `lvl' and ID `id', wants to go out from its gnode
        G of level lvl+1. We are the coordinator of this gnode G.

        If gfree_new is None, then we don't have to check any condition.
        So we remove the node and return the new free_nodes of this gnode.

        Otherwise, the caller has passed the number of free nodes in the
        gnode where it's going to rehook. So we must check that our
        free_nodes is lesser than (gfree_new - 1).
        If so, we remove the node and return the new free_nodes of this gnode,
        else, we return None."""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if gfree_new is None \
                or self.mapcache.free_nodes_nb(lvl) < gfree_new - 1:
            if self.mapcache.node_get(lvl, id).alive:
                self.mapcache.node_del(lvl, id)
                return self.mapcache.free_nodes_nb(lvl)
            else:
                return None
        else:
            return None

    def going_out_ok(self, lvl, id):
        """The node, which was going out, is now acknowledging the correct
        migration"""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        self.mapcache.tmp_deleted_del(lvl, id)

    def going_in(self, lvl, gfree_old_coord=None):
        """A node wants to become a member of our gnode G of level `lvl+1'.
        We are the coordinator of this gnode G (so we are also a member of G).

        If gfree_old_coord is None, then we don't have to check any condition.
        So we add the node and return the assigned newnip.

        Otherwise, the caller has passed the current number of free nodes
        in the gnode which it's leaving. So we must check that our
        current free_nodes is bigger than gfree_old_coord.
        If so, we add the node and return the assigned newnip,
        else, we return None."""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        logging.log(logging.ULTRADEBUG, 'Coord.going_in: The requested '
                                        'level is ' + str(lvl))
        logging.log(logging.ULTRADEBUG, 'Coord.going_in: This is mapcache.')
        logging.log(logging.ULTRADEBUG, self.mapcache.repr_me())

        if gfree_old_coord is not None \
                and not self.mapcache.free_nodes_nb(lvl) > gfree_old_coord:
            return None

        fnl = self.mapcache.free_nodes_list(lvl)
        if fnl == []:
                return None

        newnip = self.mapcache.me[:]
        newnip[lvl] = choice(fnl)
        for l in reversed(xrange(lvl)): newnip[l] = choice(valid_ids(lvl, 
                                                                     newnip))

        self.mapcache.alive_node_add(lvl, newnip[lvl])

        logging.log(logging.ULTRADEBUG, 'Coord.going_in: returns ' + 
                    str(newnip))
        return newnip

