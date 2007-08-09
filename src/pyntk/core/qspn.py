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

import sys
sys.path.append("..")
from lib.event import Event
from utils.misc import unique

class Etp:

    def __init__(self, neigh, maproute):

    	self.maproute=maproute

    	neigh.events.listen('NEW_NEIGH', self.etp_new_changed)
    	neigh.events.listen('REM_NEIGH', self.etp_new_changed)
    	neigh.events.listen('DEL_NEIGH', self.etp_new_dead)
    
    @microfunc(True)
    def etp_new_dead(self, neigh, oldrem=None):
	"""Builds and sends a new ETP for the worsened link case.
	
	If oldrem=None, the node `neigh' is considered dead."""
    	
	## Create R
	R=self.maproute.bestroutes_get()
	def gw_is_neigh((dst, gw, rem)):
		return gw == neigh.id
	for L in R:
		L=filter(gw_is_neigh, L)
	##

	## Update the map
	self.maproute.routeneigh_del(neigh)
	##
    	
    	## Create R2
	def rem_or_none(r):
		if r!=None:
			return r.rem
		return DeadRem()

	R2 = [ 
	      [ (dst, rem_or_none(self.maproute.node_get(lvl, dst).best_route())
		    for dst,gw,rem in R[lvl]
	      ] for lvl in xrange(self.maproute.levels)
	     ]
	#Note: if rem is None, then its relative route is dead
	##

	## Forward the ETP to the neighbours
	flag_of_interest=1
	TP = [(self.me, None)]	# Tracer Packet included in the first block of
				# the ETP
	etp = (R, [(0, TP)], flag_of_interest)
	self.etp_forward(etp, [neigh.id])
	##

    @microfunc(True)
    def etp_new_improved(self, neigh, oldrem=None):
	"""Builds and sends a new ETP for the improved link case
	
	If oldrem=None, the node `neigh' is considered new."""

	## Update the map
    	if oldrem == None:
    		self.maproute.routeneigh_add(neigh)
    	else:
    		self.maproute.routeneigh_rem(neigh)
	##

	## Create R
	R=self.maproute.bestroutes_get()
	def gw_is_neigh((dst, gw, rem)):
		return gw != neigh.id
	for L in R:
		L=filter(gw_is_neigh, L)
	##

	## Fix R
	def takeoff_gw((dst, gw, rem)):
		return (dst, rem)
	R=map(takeoff_gw, R)
	##

	## Send the ETP to `neigh'
	flag_of_interest=1
	TP = [(self.me, None)]	# Tracer Packet included in the first block of
				# the ETP
	etp = (R, [(0, TP)], flag_of_interest)
	neigh.ntkd.etp.etp_exec(self.maproute.me, *etp)
	##

    @microfunc()
    def etp_exec(self, sender_nip, R, TPL, flag_of_interest):
        """Executes a received ETP"""

	def isnot_empty(x):return x!=[] #helper func
        
        gwip    = sender_nip
	neigh   = self.neigh.ip_to_neigh(gwip)
	gw	= neigh.id
	gwrem	= neigh.rem

	## Group rule
	level = self.maproute.nip_cmp(self.me, gwip)
	for block in TPL:
		lvl = block[0] # the level of the block
		if lvl < level:
			block[0] = level
			blockrem = sum(rem for hop, rem in block[1][1:])
			block[1] = [(gwip[level], blockrem)]
			R[lvl] = []
        
	
	### Collapse blocks of the same level
	TPL2=[]
	precblk=None
	for block in TPL:
		if block[0] == preclvl:
			precblk[1]+=block[1]
		else:
			if precblk != None:
				TPL2.append(precblk)
			precblk = block
        TPL=TPL2
	###
	
	### Remove dups
	def remove_contiguos_dups_in_list(L):
		L2=[]
		prec=None
		for x in L:
			if x != prec:
				prec=x
				L2.append(x)
		return L2

	for block in TPL:
		block[1]=remove_contiguos_dups_in_list(block[1])
	###

	##

	## ATP rule
	for block in TPL:
		if self.me[block[0]] in block[1]:
			return
	##

	tprem = sum(rem for hop, rem in TP[1:])
	
	## Update the map
	for lvl in xrange(self.maproute.levels):
		for dst, rem in R[lvl]:
			if not self.maproute.route_rem(lvl, dst, gw, rem+tprem):
				self.maproute.route_change(lvl, dst, gw, rem+tprem)
        ##

	## S
	S = [ [ (dst, r.rem)
		for dst, rem in R[lvl]
		    for r in [self.maproute.node_get(lvl, dst).best_route()]
		        if r.gw != gw
	      ] for lvl in xrange(self.maproute.levels) ]

	if flag_of_interest:
		if sum(filter(isnot_empty, S)) != 0:  # <==> if S != [[], ...]:  
						      # <==> S isn't empty
			flag_of_interest=0
			TP = [(self.me, None)]	
			etp = (S, [(0, TP)], flag_of_interest)
			neigh.ntkd.etp.etp_exec(self.maproute.me, *etp)
	##

	## R2 
	R2 = [ [ (dst, rem)
		for dst, rem in R[lvl]
		    if dst not in [d for d, r in S[lvl]]
	      ] for lvl in xrange(self.maproute.levels) ]
	##

	if sum(filter(isnot_empty, R2)) != 0:  # <==> if R2 isn't empty
		if TPL[-1][0] != 0:
			TP = [(self.me, None)]	
			TPL.append((0, TP))
		else:
			TPL[-1][1].append((self.me, gwrem))
		etp = (R2, TPL, flag_of_interest)
		self.etp_forward(etp, [neigh.id])

    def etp_forward(self, etp, exclude):
        """Forwards the `etp' to all our neighbours, exclude those contained in `exclude'"""

	for nr in self.neigh_list():
		nr.ntkd.etp.etp_exec(self.maproute.me, *etp)

