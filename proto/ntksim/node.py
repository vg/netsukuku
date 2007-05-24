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
import random
from maps import *

#class node
class node:
		
	def __init__(self, nodeid, rnode,rem):
		
		self.rnodes={}	#each element is a link (neghibour id; (l_id, rem))
				
		self.nid=int(nodeid) #node ID
		self.ip=[0 for i in range(G.NUM_OF_LEVELS)]
		
		link=link_t(rem) 	#create the link for with the neighbour rnode
		self.rnodes[rnode]=link 
		
		self.tracer_forwarded=0
		self.etracer_forwarded=0
		
		self.dp=0.0 #death probability
		self.dead=0 #node_dead
		
		self.int_map={} #The element of this list will be metric_array_t elements
				#Only alive nodes are kept in this map
		self.dead_nodes=[]	# list of dead nodes present in
					# the int map
	
	def print_node(self):
		rlen=self.num_of_active_neigh()
		print "nodeid=", self.nid, "number of rnodes:", rlen, "ip:", self.ip, "death prob:", self.dp
		
		#for a,b in self.rnodes.iteritems():
		#	if G.whole_network[a].dead!=1:
		#		print "\tneighour:", a
	
	def get_id(self):
		return self.nid
	
	
	def new_link(self,rnode,rem):
		link=link_t(rem)
		self.rnodes[rnode]=link
	
	def get_level_id(self,level):
		return self.ip[level]
	
	def get_ip(self):
		return self.ip
	
	def num_of_active_neigh(self):
		num=0
		for a,b in self.rnodes.iteritems():
			if G.whole_network[a].dead!=1:
				num+=1
		return num
		
	def create_gnodes(self,level,ip_add):
		gnode=map_gnode(level,ip_add)
		gnode.seeds+=1
	
	def find_free_ip(self,neig_add):
		
		ok=0
		for i in range(1,G.NUM_OF_LEVELS):
			
			#pdb.set_trace()
			
			temp_gnode=G.network[(i,neig_add[i])] #take an istance of the gnode who belongs to
			
			while (temp_gnode.seeds!=G.MAXGROUPNODE):
				
				rand=random.randrange(1,G.MAXGROUPNODE+1)
				
				if (temp_gnode.ip_is_used(rand)==0): #if rand is free
					ok=1
					break
			
			if (temp_gnode.seeds!=G.MAXGROUPNODE)&(ok==1):
			
				if i==1:
					for j in range (0,i):
						self.ip[j]=rand	#assing rand
					
					temp_gnode.ip_used.append(rand)	#store that rand it was taken
					temp_gnode.seeds+=1
					
					j=i
					for j in range (i,G.NUM_OF_LEVELS):
						self.ip[j]=neig_add[j]
					return 1
				else:
					for l in range(0,i-1):
						self.ip[l]=random.randrange(1,G.MAXGROUPNODE+1)
					self.ip[i-1]=rand
					for l in range(i,G.NUM_OF_LEVELS):
						self.ip[l]=neig_add[l]
					self.create_gnodes(i-1,self.ip)
			else:
				print "create a new gnode at level:", i
				
		if ok==0:
			return 0

	def hook(self):
		neigh=0
		print "hooking node:", self.nid
		
		for node,link in self.rnodes.iteritems():	#for all its neighbours
			if G.whole_network.has_key(node):
				if G.whole_network[node].dead!=1:
					neigh=1
					neigh_ip=G.whole_network[node].get_ip()
					ok=self.find_free_ip(neigh_ip)
					if ok==1:
						break
					else:
						neigh=0
			
		if neigh==0:		#no existing neighbour	 
			self.ip[0]=random.randrange(1,G.MAXGROUPNODE+1)	
			for i in range(1,G.NUM_OF_LEVELS):
				self.ip[i]=random.randrange(1,G.MAXGROUPNODE+1)
				self.create_gnodes(i,self.ip)

	#PRINT NODE INFORMATION
	#	self.print_node()
	
	
	def send_fst_packet(self):
		
		for node,link in self.rnodes.iteritems():	
			#for all its neighbours -- each element is a link (neghibour id; (l_id, rem))
			if G.whole_network[node].dead!=1:
				pack=packet(self,G.whole_network[node],"QSPN")		#create a new packet 
				pack.add_chunk(link)		#add its personal chunk
				
				delay=link.l_rem.rtt
				pack.time=G.curtime+delay
				
				pack.send_packet()
				#pkt.print_packet()

	def add_route(self, route):
		"""Returns 1 if the route has been added, otherwise 0"""

		#the element self.int_map[dest].metric_array["rtt"].gw[0].nid 
		#is the id of the gateway with the short rtt 
		#the element self.int_map[dest].metric_array["bw"].gw[0].nid 
		#is the id of the gateway with the best bw
		
		route_interesting=False
		
		if self.int_map.has_key(route.dst.nid):
						
			#self.int_map[route.dst.nid].metric_array["metric"] is a map_gw_t object		
			for metric in G.metrics:			
				dnode=self.int_map[route.dst.nid]
			
				#if dnode.metric_array[metric]:	if doesn't exist it's better the neighbour
				length=len(dnode.metric_array[metric].gw)
				#compare the worst old with the new one
				dnode_worst_rt=dnode.metric_array[metric].gw[(length-1)]
				
				#GW Optimization 
#				if route.gw.nid == r1.gw.nid:
#					print "gw is the same, skipping"
#					res = 0

				#res=self.cmp_routes(dnode_worst_rt,route,metric)
				#if res == -1:
				route.cmp_metric=metric
				if dnode_worst_rt < route:
					route_interesting=True
					#the old is worse than the new one
					if G.verbose:
						print "a better route for the node: ",\
							route.dst.nid, "for the metric: ",\
							metric, "has been found!"
						print "OLD: ", dnode_worst_rt
						print "NEW: ", route


					dnode.metric_array[metric].gw.append(route)
					dnode.metric_array[metric].sort_gw(metric)
					#remove the last element if the number of route exceed MAX_ROUTES
					if len(dnode.metric_array[metric].gw)>G.MAX_ROUTES:
						dnode.metric_array[metric].gw.pop(G.MAX_ROUTES)
		else:	
			if G.verbose:
				print "a new metric_array for the node:",route.dst.nid,\
				"for all the metrics has been created!"
			
			self.int_map[route.dst.nid]=metric_array_t(route)
			route_interesting=True
		
		return route_interesting

	def forall_routes_through(self, gw, f, tpmask=0):
		"""For each route r with gateway `gw', contained in
		   self.int_map.[nid].metric_array[metric].gw, it calls f(nid, metric, r)
		   If `tpmask' is not zero, f(..., r) will be called only if
		   r.tpmask == tpmask
		"""
		for nid, marray in self.int_map.items():
		# - note -
		# we cannot use .iteritems() because f() might delete
		# some elements from the dictionary during the for
		# - note -

			if nid == self.nid or nid == gw.nid:
				#skip myself, skip `gw'
				continue
			for metric in G.metrics:
				for route in marray.metric_array[metric].gw:
				    #print "XXX: ", route.dst.nid,\
				    # 	route.gw.nid, gw.nid, dec2bin(route.tpmask),\
				    # 	dec2bin(tpmask)
				    if route.gw.nid == gw.nid and\
				    	(not tpmask or (tpmask and\
							route.tpmask==tpmask)):
						# this is a route passing 
						# through `rnode', call f()
						f(nid, metric, route)
	def get_all_routes_bydst(self, dst):
		"""Returns the list containing all the routes with destination
		   `dst'"""
		rlist=[]
		for metric in G.metrics:
			for route in self.int_map[dst.nid].metric_array[metric].gw:
				if route.dst.nid==dst.nid:
					rlist.append(route)
		return rlist

	def del_node(self, node):
		# delete `node' from the intmap
		if self.int_map.has_key(node.nid):
			if G.debug:
				print "(%d) the node %d is dead, sigh"%(self.nid, node.nid)
			self.dead_nodes.append(node.nid)
			del self.int_map[node.nid]
	def is_dead(self, nodeid):
		return (nodeid in self.dead_nodes)

	def update_map(self, change, rnode):
		# The arguments are the same of {-build_etp-}
		
		if change == etp_section.CHANGE_DEAD_NODE:
			#Deletes from the map the node `rnode' and all the routes
			#using `rnode' as gw
			self.del_node(rnode)
			self.update_map_deadnode(rnode)
		else:
			NOT_IMPLEMENTED

	_update_map_deadnode_deleted=0 #ugly hack
	def update_map_deadnode(self, rnode, tpmask=0):
		"""Deletes from the map all the routes having as gw `rnode'

		   If tpmask!=0, the routes will be deleted only if their
		      tpmask is set to `tpmask'
		   else all the routes with gw `rnode' are removed 
		   
		   It returns 1 if some routes have been deleted
		   """
		global _update_map_deadnode_deleted
		_update_map_deadnode_deleted=0
		mapgw_set=set()

		def del_dead_route(nid, metric, route):
			global _update_map_deadnode_deleted
		
			if G.debug:
				print "del_dead_route: %d,%s,%s"%(nid,metric,str(route))
			
			mgw=self.int_map[nid].metric_array[metric].gw
			mgw.remove(route)
			_update_map_deadnode_deleted=1
			if mgw == []:
				empty=1
				for metric in G.metrics:
					if self.int_map[nid].metric_array[metric].gw != []:
						empty=0
					break
				if empty:
					# We don't have any route to
					# reach `node'. Delete it from
					# our int_map
					del self.int_map[nid]
			else:
				mapgw_set.add(mgw)

		# Delete dead routes
		self.forall_routes_through(rnode, del_dead_route, tpmask)

		#sort what we've changed
		for mgw in mapgw_set:
			mgw.sort_gw()
		return _update_map_deadnode_deleted
