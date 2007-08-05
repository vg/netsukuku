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

from event import *

class map:

    def __init__(self, levels, gsize, dataclass):
	"""Initialise the map."""
	
	self.levels = levels	# Number of levels
	self.gsize  = gsize	# How many nodes are contained in a gnode
	self.dataclass = dataclass

	# The member self.node[l][i] is a node of level l and its ID is i
	self.node = [[None for i in xrange(gsize)] for i in xrange(levels)]

	self.events = event( send_ev = [ 'NEW_NODE', 'DEL_NODE' ],
			     recv_ev = None )

    def node(self, lvl, id):
	"""Returns from the map a node of level `lvl' and id `id'.

	A class instance of type `self.dataclass' will always be returned: if
	it doesn't exist, it is created"""
	
	if self.node[lvl][id] == None:
		self.node[lvl][id]=self.dataclass(lvl, id)
	return self.node[lvl][id]

    def node_del(self, lvl, id):
        if self.node[lvl][id] is not None:
	    	self.events.send('DEL_NODE', (lvl, id))
	self.node[lvl][id]=None
