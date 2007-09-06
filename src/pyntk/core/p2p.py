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
# Implementation of the P2P Over Ntk RFC. See {-P2PNtk-}
#

import sys
sys.path.append("..")
from lib.event import Event
from core.map import Map

def identity(x): return x

class PartecipantNode:
    def __init__(self, 
		 lvl=None, id=None  # these are mandatory for Map.__init__(),
		):
	
    	self.partecipant = False
    
    def _pack(self):
        return (self.partecipant,)
    def _unpack(self, (p,)):
        ret=PartecipantNode()
	ret.partecipant=p
	return ret

class MapP2P(Map):
   """Map of the partecipant nodes"""

   def __init__(self, levels, gsize, me, pid, h):
       """levels, gsize, me: the same of Map

	  pid: P2P id of the service associated to this map
	  h  : the function h:KEY-->IP"""
	
    	Map.__init__(self, levels, gsize, PartecipantNode, me)

	self.pid = pid
	self.h = h

    def H(self, key):
        """This is the function that maps each key to an existent hash node
	
	   If there are no partecipants, None is returned"""

	IP = self.h(key)

	hIP = [None]*self.levels
	for l in reversed(xrange(self.levels)):
		for id in xrange(self.gize):
			for sign in [-1,1]:
				hid=(IP[l]+id*sign)%self.gsize
				if self.node_get(l, hid).partecipant:
					hIP[l]=hid
					break
			if hIP[l]:
				break
		if hIP[l] is None:
			return None

		if hIP[l] != self.me[l]:
			# we can stop here
			break
	return hIP

    def partecipate(self):
        """self.me is now a partecipant node"""

	for l in xrange(self.levels):
		self.node[l][self.me[l]].partecipant = True

    @microfunc()
    def _me_changed(self, old_me, new_me):
        return self.me_change(new_me)

    @microfunc(True)
    def _node_del(self, lvl, id):
        return self.node_del(lvl, id)


class P2P:
    def __init__(self, radar, maproute,
		 pid, h=identity):
	  """radar, maproute, hook: the instances of the relative modules

	  pid: P2P id of the service associated to this map
	  h  : the function h:KEY-->IP"""

	self.radar    = radar
    	self.neigh    = radar.neigh
    	self.maproute = maproute

	self.mapp2p   = MapP2P(self.maproute.levels, self.maproute.gsize, self.maproute.me, 
				pid, h)
	
        self.maproute.events.listen('ME_CHANGED', self.mapp2p._me_changed)
        self.maproute.events.listen('NODE_DELETED', self.mapp2p._node_del)

        # are we a partecipant?
	self.partecipant = False

    def neigh_get(self, hip):
        """Returns the Neigh instance of the neighbour we must use to reach
	   the hash node.
	   `hip' is the IP of the hash node.
	   
	   If nothing is found, None is returned"""
	
	lvl = self.mapp2p.nip_cmp(hip, self.mapp2p.me)
	return self.neigh.id_to_neigh(self.maproute.node_get(lvl,hip[lvl]).best_route().gw)

    def partecipate(self):
        """Let's become a partecipant node"""

	self.mapp2p.partecipate()

	for nr in self.neigh.neigh_list():
		nr.ntkd.p2p.partecipant_add(self.pid, self.me)

    def partecipant_add(self, pIP):
	continue_to_forward = False

        lvl = self.nip_cmp(pIP, self.mapp2p.me)
	for l in xrange(lvl, self.mapp2p.levels):
		if not self.mapp2p.node[l][pIP[l]].partecipant:
			self.mapp2p.node[l][pIP[l]].partecipant=True
			continue_to_forward = True

	if not continue_to_forward:
		return

	for nr in self.neigh.neigh_list():
		nr.ntkd.p2p.partecipant_add(self.pid, pIP)

class P2PAll:
    """Class of all the registered P2P services"""

    def __init__(self, radar, maproute, hook)
        self.radar = radar
	self.maproute = maproute
	self.hook = hook

        self.service = {}

	self.events=Event(['P2P_HOOKED'])

	self.hook.listen('HOOKED', self.p2p_hook)

    def add(self, pid, h=identity):
        self.service[pid] = P2P(self.radar, self.maproute, self.hook, pid, h)
	return self.service[pid]

    def del(self, pid):
        if pid in self.service:
		del self.service[pid]
    
    def get(self, pid):
        if pid not in self.service:
		return self.add(pid)
	else:
		return self.service[pid]

    def getall(self):
        return [(s, self.service[s].mapp2p.map_data_pack()) 
			for s in self.service]

    def partecipant_add(self, pid, pIP):
        self.get(pid).partecipant_add(pIP)

    @microfunc()
    def p2p_hook(self, *args):
        """P2P hooking procedure
	
	It gets the P2P maps from our nearest neighbour"""

	## Find our nearest neighbour
	minlvl=self.levels
	minnr =None
	for nr in self.neigh.neigh_list():
		lvl=self.nip_cmp(self.me, self.ip_to_nip(nr.ip))
		if lvl < minlvl:
			minlvl = lvl
			minnr  = nr
        ##

	if minnr == None:
		# nothing to do
		return

	nrmaps_pack = minnr.ntkd.p2p.getall()
	for (pid, map_pack) in nrmaps_pack:
		self.get(pid).mapp2p.map_data_merge(*map_pack)
       
        for s in self.service:
		if self.service[s].partecipant:
			self.service[s].partecipate()

	self.events.send('P2P_HOOKED', ())
