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

from event import Event

class Etp:

    def __init__(self, neigh, maproute):

    	self.maproute=maproute

    	neigh.events.listen('NEW_NEIGH', etp_new_improved)
    	neigh.events.listen('REM_NEIGH', etp_new)
    	neigh.events.listen('DEL_NEIGH', etp_new_worsened)
    	

    def etp_new(self, neigh, oldrem):
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
	R2 = []
	lvl=0
	for L in R:
		RL=[]
		for dst,gw,rem in L:
			r=self.maproute.node_get(lvl, dst).best_route()
			if r != None:
				RL.append((dst, r.rem))
		R2.append(RL)
		lvl+=1
	##

	## Forward the ETP to the neighbours
	tp = [(self.me, None)]	# Tracer Packet included in the ETP
	flag_of_interest=1
	etp = (R2, TP, flag_of_interest)
	etp_forward(etp, [neigh.id])
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

	## Send the ETP to `neigh'
	tp = [(self.me, None)]	# Tracer Packet included in the ETP
	flag_of_interest=1
	etp = (R2, TP, flag_of_interest)
	neigh.ntkd.etp.etp_exec(etp)
	##

    def etp_forward(self, etp, exclude):
        """Forwards the `etp' to all our neighbours, exclude those contained in `exclude'"""

	for nr in self.neigh_list():
		nr.ntkd.etp.etp_exec(etp)

    def etp_exec(self, (R, TP, flag_of_interest)):
	#TODO
        pass
