#
#  This file is part of Netsukuku, it is based on q2sim.py
#
#  ntksim
#  (c) Copyright 2007 Alessandro Ordine
#
#  q2sim.py
#  (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# 
#  This source code is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as published 
#  by the Free Software Foundation; either version 2 of the License,
#  or (at your option) any later version.
# 
#  This source code is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  Please refer to the GNU Public License for more details.
# 
#  You should have received a copy of the GNU Public License along with
#  this source code; if not, write to:
#  Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# 


import pdb
from maps import *
from G import *
from node import *
from rem import *
from misc import *


####
########  ROUTE  
####

class route:
	cmp_metric=G.metrics[0]

	def __str__(self):
		return "dst="+str(self.dst.nid)+" src="+str(self.src.nid)+" gw="+str(self.gw.nid)+" trtt="+str(self.rem.rtt)+" bwup="+str(self.rem.bwup)+" dp="+str(self.rem.dp)

	def __init__(self, src_id, dst_id, gw_id, trc, pktpayload):
		"""
		`pktpayload' is used just to recover the rem and link_id info.
		`trc' is what we actually use: it is an array of node IDs
		"""
		
		self.src=G.whole_network[src_id]
		self.dst=G.whole_network[dst_id]
		self.gw=G.whole_network[gw_id]
		self.tpmask=0
		
		trtt=0
		bw_up=999999
		bw_dw=999999
		tot_dp=0
		
		self.link_id=[]
		self.hops=[]

		ok=0
		for i in reversed(trc):
			self.tpmask|=1<<i
			
			for j in reversed(pktpayload.chunks): 
				#search  for all chunks in the packet starting from the
				#end to avoid strange beavior with the cycles
				if (j.nid==i):
					#x-->y-->z
					#src=z
					#dst=x
					#j.chunk.rem.bwdw are bw_yx and bw_zy
					#j.chunk.rem.bwup are bw_xy and bw_yz
					#bw_up are min={bw_yx,bw_zy} 
					
					#consider that the direction of the packet is 
					#the opposite of the direction of the route 
					
					#alessandro 1/bw modification 5-21-2007
					"""
					if j.chunk.rem.bwdw<bw_up:
						bw_up=j.chunk.rem.bwdw
					if j.chunk.rem.bwup<bw_dw:
						bw_dw=j.chunk.rem.bwup
					"""
					
					#alessandro 1/bw modification 5-21-2007
					bw_up+=j.chunk.rem.bwdw
					bw_dw+=j.chunk.rem.bwup	
						
						
						
					#dp is min={dp_x,dp_y} 
					if j.ndp>tot_dp:
						tot_dp=j.ndp
					
					trtt+=j.chunk.rem.rtt					
					#self.link_id.append(j.chunk.linkid)
							
					ok=1 
					break  #if you found it run away, 
					       #there should be other chunks due to the cicles, 
					       #in this case we want the closer to the node
			
			if ok==0 :
				EHM_SOMETHING_WRONG
				
			if i!=dst_id:
				self.hops.append(G.whole_network[i])
			else:
				break
		
		self.rem=rem_t(trtt,bw_up,bw_dw,tot_dp)
	
	def __cmp__(self,b):
		"""Remember to set route.cmp_metric before comparing two
		   routes!!!"""
		if route.cmp_metric not in G.metrics:
			EHM_SOMETHING_WRONG

		rem_t.cmp_metric=route.cmp_metric
		return self.rem.__cmp__(b.rem)
	
	
	def print_route(self):
		print "Route:: src:",self.src.nid, "dst:",self.dst.nid,	"gw:",self.gw.nid,\
				"tpmask: ",dec2bin(self.tpmask)
		print "        trtt:",self.rem.rtt, "bw_up:",self.rem.bwup,\
			"bw_dw:",self.rem.bwdw,"avg:",self.rem.avg
		print "        dp%:",(self.rem.dp)*100
		print "        hops:",
		for i in self.hops:
			print i.nid,
		print ""
#		print "        link_id:",
#		for i in self.link_id:
#			print i,
