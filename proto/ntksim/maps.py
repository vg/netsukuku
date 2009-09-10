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

from G import *
from packet import *
from rem import *

#Link Class
class link_t:
	def __init__(self, rem):
		self.l_id=0	#link id
		self.l_rem=rem
		self.link_dead=0 #link dead
		
	def print_link(self):
		print "\tl(id):", self.l_id, "rtt:", self.l_rem.rtt, "bw:", self.l_rem.bw
		
#map_gnode class
#structure of a group node

class map_gnode:
	def __init__(self,level,ip_addr):
		
		self.ip_used=[]			#list of the gnode ids used in this gnode
		self.ip=[0 for i in range(G.NUM_OF_LEVELS)] 	#initialization of ip defaul 0.0.0.0 
		
		for i in range(level):
			self.ip[i]=0	#put 0 in the sublevels 
		for i in range(level,G.NUM_OF_LEVELS):
			self.ip[i]=ip_addr[i] #set the gnode id
		
		self.ip_used.append(ip_addr[level-1]) 	#write that level-1 id  is used in its gnodes
		G.network[(level,ip_addr[level])]=self	
			
		#self.flags=0	#uncomment it if you need flags
		self.seeds=0		#the number of the active (g)nodes forming this gnode
	
	def ip_is_used(self,num):
		if num not in self.ip_used:
			return 0
		else:
			return 1


#map_gw_t
class map_gw_t:
	def __init__(self,route):
		self.gw=[]
		self.gw.append(route)			#pointer to the node
	
	def sort_gw(self, metric):
		route.cmp_metric=metric
		self.gw.sort(reverse=1)

class metric_array_t:
	
	def __init__(self,route):		
		
		#it is executed only when the entry isn't present 
		#the route is the best for all the metrics
		
		self.metric_array={}
  		self.metric_array["rtt" ] = map_gw_t(route)
  		self.metric_array["bwup"] = map_gw_t(route)
  		self.metric_array["bwdw"] = map_gw_t(route)
  		self.metric_array["dp"  ] = map_gw_t(route)
		self.metric_array["avg" ] = map_gw_t(route)


class ext_map:
	def __init__(self):
		gmap=[]		#gmap points to the gmap of level _NL(x)
		max_metric_routes=0
		root_gnode=0	#it will contain the root_gnodes of each level of the ext_map


