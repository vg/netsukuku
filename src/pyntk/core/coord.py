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

import sys
sys.path.append("..")
from lib.micro import microfunc
from lib.event import Event
from wrap.xtime import time
from core.p2p import P2P
from core.map import Map
from random import choice


class Node:
    def __init__(self, 
		 lvl=None, id=None  # these are mandatory for Map.__init__(),
		):
	
    	self.alive = False

    def is_free(self):
        return self.alive

    def _pack(self):
        return (self.alive,)

    def _unpack(self, (p,)):
        ret=PartecipantNode()
	ret.alive=p
	return ret

class MapCache(Map):
    def __init__(self, maproute):
        Map.__init__(self, maproute.levels, maproute.gsize, Node, maproute.me)

	self.copy_from_maproute(maproute)
	self.remotable_funcs = [self.map_data_merge]

	self.tmp_deleted = {}

    def copy_from_maproute(self, maproute):
        for lvl in xrange(self.levels):
		for id in xrange(self.gsize):
			if not maproute.node_get(lvl, id).is_empty():
				self.node_add(lvl, id)

    def node_add(self, lvl, id):
        if self.node_get(lvl, id).alive:
		Map.node_add(self, lvl, id)
		self.node_get(lvl, id).alive = True

    def node_del(self, lvl, id):
        if self.node_get(lvl, id).alive:
		Map.node_del(self, lvl, id)
		self.node_get(lvl, id).alive = False

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

    def __init__(self, radar, maproute, p2pall):
	
        P2P.__init__(self, radar, maproute, Coord.pid)

        # let's register ourself in p2pall
        p2pall.p2p_register(self)

        # The cache of the coordinator node
        self.mapcache = MapCache(self.maproute)

	self.maproute.events.listen('NODE_NEW', self.mapcache.node_add)
	self.maproute.events.listen('NODE_DELETED', self.mapcache.node_del)

	self.mapp2p.events.listen('NODE_NEW', self.new_partecipant_joined)

	self.coordnode = [None]*self.maproute.levels
	self.coornodes_set()

	self.remotable_funcs = [self.going_out, self.going_out_ok, self.going_in]

    def h(self, (lvl, ip)):
        """h:KEY-->IP"""
	IP = ip
	for l in reversed(xrange(lvl)): IP[l] = 0
	return IP

    def coornodes_set(self):
        """Sets the coordinator nodes of each level, using the current map"""
	for lvl in xrange(self.maproute.levels):
		self.coordnode[lvl] = self.H(self.h((lvl, self.maproute.me)))

    @microfunc()
    def new_partecipant_joined(self, lvl, id):
        """Shall the new partecipant succeed us as a coordinator node?"""

	# the node joined in level `lvl', thus it may be a coordinator of the
	# level `lvl+1'
	level = lvl+1

	pIP = self.maproute.me
	pIP[lvl] = id
	for l in reversed(xrange(lvl)): pIP[l]=None

	newcor = self.H(self.h(level, self.maproute.me))
	if newcor != pIP:
		# the new partecipant isn't a coordinator node
		return

	oldcor = self.coordnode[level]
	self.coordnode[level] = newcor

	if oldcor == self.maproute.me and pIP != self.maproute.me:
	#if we were a coordinator node, and it is different from us:
		# let's pass it our cache
		peer = self.peer(hIP=newcor)
		peer.mapp2p.map_data_merge(self.mapp2p.map_data_pack())

    def going_out(self, lvl, id, gnumb=None):
        """The node of level `lvl' and ID `id', wants to go out from its gnode
	   G of level lvl+1. We are the coordinator of this gnode G.
	   We'll give an affermative answer if `gnumb' < |G| or if
	   `gnumb'=None"""

        
	if (gnumb < self.mapp2p.nodes_nb[lvl]-1 or gnumb == None)	\
		and self.mapp2p.node_get(lvl, id).alive:
		self.mapp2p.node_del(lvl, id)
		return self.mapp2p.nodes_nb[lvl]
	else:
		return None

    def going_out_ok(self, lvl, id):
        """The node, which was going out, is now acknowledging the correct
	migration"""
	self.mapp2p.tmp_deleted_del(lvl, id)

    def going_in(self, lvl, gnumb=None):
        """A node wants to become a member of our gnode G of level `lvl+1'.
	   We are the coordinator of this gnode G (so we are also a member of G).
	   We'll give an affermative answer if `gnumb' > |G| or if
	   `gnumb'=None"""

	if gnumb > self.mapp2p.nodes_nb[lvl]+1:
		fnl = self.mapp2p.free_nodes_list(lvl)
		if fnl == []:
			return None

		newip = self.mapp2p.me
		newip[lvl] = choice(fnl)
		for l in reversed(xrange(lvl)): newnip[l]=randint(0, self.mapp2p.gsize)
		self.node_add(lvl, newip[lvl])
		return newip

	return None
