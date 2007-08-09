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

    	neigh.events.listen('NEW_NEIGH', etp_new_improved)
    	neigh.events.listen('REM_NEIGH', etp_new)
    	neigh.events.listen('DEL_NEIGH', etp_new_worsened)
    	

    def etp_new(self, neigh, oldrem):
        #TODO: non ha piu' senso questa distinzione
    	if oldrem < neigh.rem:
    		etp_new_improved(neigh, new=0)
    	else:
    		etp_new_worsened(neigh, dead=0)

    def etp_new_worsened(self, neigh, oldrem=None):
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
    	if oldrem == None:
    		self.maproute.routeneigh_del(neigh)
    	else:
    		self.maproute.routeneigh_rem(neigh, oldrem)
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
	TP = [(self.me, None)]	# Tracer Packet included in the ETP
	flag_of_interest=1
	etp = (R2, TP, flag_of_interest)
	self.etp_forward(etp, [neigh.id])
	##

    def etp_new_improved(self, neigh, oldrem=None):
	"""Builds and sends a new ETP for the improved link case
	
	If oldrem=None, the node `neigh' is considered new."""

	## Update the map
    	if oldrem == None:
    		self.maproute.routeneigh_add(neigh)
    	else:
    		self.maproute.routeneigh_rem(neigh, oldrem)
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
	TP = [(self.me, None)]	# Tracer Packet included in the ETP
	flag_of_interest=1
	etp = (R, TP, flag_of_interest)
	neigh.ntkd.etp.etp_exec(etp)
	##

    def etp_exec(self, R, TP, flag_of_interest):
        """Executes a received ETP"""

	def isnot_empty(x):return x!=[] #helper func
        
	def group_rule(TP, lvl):
		#TODO....
		return gTP, lvl

        gwip    = TP[-1]
	neigh   = self.neigh.ip_to_neigh(gwip)
	gw	= neigh.id
	gwrem	= neigh.rem

	TP+=[(self.me, gwrem)]
	self.group_rule(TP)
	tprem = sum(rem for hop, rem in TP[1:])
	
	## Update the map
	for lvl in xrange(self.maproute.levels):
		for dst, rem in R[lvl]:
			if not self.maproute.route_rem(lvl, dst, gw, rem+tprem):
				self.maproute.route_change(lvl, dst, gw, rem+tprem)
        #TODO: update from the TP too!
        ##
	
	S = [ [ (dst, r.rem)
		for dst, rem in R[lvl]
		    for r in [self.maproute.node_get(lvl, dst).best_route()]
		        if r.gw != gw
	      ] for lvl in xrange(self.maproute.levels) ]

	if flag_of_interest:
		if sum(filter(isnot_empty, S)) != 0:  # <==> if S != [[], ...]:  
						      # <==> S isn't empty
			flag_of_interest=0
			etp = (S, [(self.me, None)], flag_of_interest)
			neigh.ntkd.etp.etp_exec(etp)

	## R2 
	R2 = [ [ (dst, rem)
		for dst, rem in R[lvl]
		    if dst not in [d for d, r in S[lvl]]
	      ] for lvl in xrange(self.maproute.levels) ]
	##

	if sum(filter(isnot_empty, R2)) != 0:  # <==> if R2 isn't empty
		etp = (R2, TP+[(self.me, gwrem)], flag_of_interest)
		self.etp_forward(etp, [neigh.id])

    def etp_forward(self, etp, exclude):
        """Forwards the `etp' to all our neighbours, exclude those contained in `exclude'"""

	for nr in self.neigh_list():
		nr.ntkd.etp.etp_exec(etp)

