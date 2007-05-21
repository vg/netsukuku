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
from G import *
from node import *
from maps import *
from route import *
from heapq import *
from misc import *

####
######## TRACER PACKET
####
"""
nodes xx->ll->zz

      ------------------------
      |	       pkt 	     |
      |			     |
      |	 ------------------  |
	|   tracerpkt	   |
	|		   |
	| ---------------  |
	  | QSPN Chunk yy|
	  | nid=xx	 |
	  ---------------
 	  | QSPN Chunk kk|
	  | nid=ll	 |
	  ---------------
          | QSPN Chunk hh|
	  | nid=zz       |



packet.payload.chunks[i].chunk.rem=xxx 
"""

class qspn_chunk:	
        def __init__(self,link_id,rem_):
		self.linkid=link_id  # linkid of the link between chunk[i-1].nid <--> chunk[i].nid
        	self.rem=rem_    # rem of the link chunk[i-1].nid <--> chunk[i].nid
	
	def print_qspn_ch(self):
		print "linkid:",self.linkid
		print "rem:",
		self.rem.print_rem()
	

class tracer_chunk:
	def __init__(self,nid,pkt_type,link_id,rem_,ndp_):
		self.nid=nid    		# nid -- id of the (g)node that put this chunk in the tpkt
		self.ndp=ndp_			# ndp -- death probability of the (g)node that
						#        put this chunk in the tpkt
        	self.chunktype=pkt_type		# chunktype -- type of the chunk. most likely QSPN
		if pkt_type=="QSPN":
			self.chunk=qspn_chunk(link_id,rem_)	# chunk -- pointer to the chunk
	
	def print_chuncks(self):
		print "nid:",self.nid
		print "chunktype:",self.chunktype
		if self.chunktype=="QSPN":
			self.chunk.print_qspn_ch()

class etp_section:
	# Type of changes
	CHANGE_WORSENED_LINK	=0
	CHANGE_IMPROVED_LINK	=1
	CHANGE_NEW_LINK		=2
	CHANGE_BROKEN_LINK	=3
	CHANGE_NEW_NODE		=4
	CHANGE_DEAD_NODE	=5

	def __init__(self):
		self.enabled=0	# Indicates if this Extended section is being
				# used.
		self.change=etp_section.CHANGE_WORSENED_LINK # type of change
		self.changed_node=0		# pointer to the changed node (f.e.
						# the dead or new node)
		self.interest_flag=0
		self.routes=[]		# Each element is a tuple (dst,rem,tpmask)

	def build_etp(self, me, change, rnode):
		"""
		   build_etp() is used to create and initialize the ETP.
		   This happens only once in the life time of a single ETP.

		   `me' is the node which is building this etp.
		   `rnode' is an rnode of `me'.
		   This ETP will notify the other nodes of the change of the 
		    `rnode' -- `me' link.
		   The `change' can be:
		   	0: worsened link
			1: improved link
			2: new link
			3: broken link
			4: new node: `rnode' is the new node, joining the net
			5: dead node: `rnode' the dead

		   If the ETP has been created created 1 is returned,
		   otherwise 0. The latter case happens when it's useless to
		   create the ETP.
		"""
		created=0
		self.enabled=1
		self.change=change
		self.changed_node=rnode

		if change == etp_section.CHANGE_DEAD_NODE:
			created = self._build_etp_deadnode(me, rnode)
			me.update_map(change, rnode)
			self.interest_flag=created

			# In every case, even if the change is uninteresting,
			# we must inform the other nodes of the death of
			# `rnode'
			created = 1

		elif change == etp_section.CHANGE_WORSENED_LINK:
			NOT_IMPLEMENTED
			me.update_map(change, rnode)
			created = _build_etp_wlink(me, rnode)
			self.interest_flag=created

		elif change == etp_section.CHANGE_IMPROVED_LINK:
			NOT_IMPLEMENTED
			me.update_map(change, rnode)
			created = _build_etp_ilink(me, rnode)
			self.interest_flag=created
		else:
			NOT_IMPLEMENTED

		return created
			
	def _build_etp_deadnode(self, me, rnode):
		created=1

		mapgw_list=[]
		def append_routes(nid, metric, route):
			"""function passed to me.forall_routes_through()"""
			mapgw_list.append(me.int_map[nid].metric_array[metric].gw)
		
		me.forall_routes_through(rnode, append_routes)
		
		mapgw_list=remove_duplicates(mapgw_list)
		
		# let's add in the ETP all the primary routes equivalent
		# to `route'
		for mgw in mapgw_list:
			for r in mgw:
				self.routes.append(r)
		self.routes=remove_duplicates(self.routes)

		if self.routes == []:
			created=0
		return created

class tracer_packet:
	def __init__(self,pkt_type):
		self.nchunk=0
		self.chunks=[]
		self.etp=etp_section()
		
	def print_trcpkt(self):
		print "nchunk:",self.nchunk
		for i in xrange(len(self.chunks)):
			self.chunks[i].print_chuncks()

	#
	### Extended Tracer Packet
	#
	def exec_etp(self, me):
		"""Returns 1 if the packet shall be forwarded"""

		whole_tracer=self.create_trcr()
		if G.verbose:
			print "ETP:", whole_tracer, "change:",\
				self.etp.change, "cnode:",self.etp.changed_node,\
				"iflag:", self.etp.interest_flag
		# The tpmask of the single route formed by the chunks of the TP
		tpmask=reduce(lambda x, y: x|1<<y, trcr, 0)

		# Acyclic rule
		if me.nid in whole_tracer:
			#drop it
			if G.verbose:
				print "Cycle detected, ETP completely dropped"
			return 1
		
                if self.etp.change == etp_section.CHANGE_DEAD_NODE:
			etp_interesting = self.exec_etp_deadnode(me, whole_tracer, tpmask)
                elif self.etp.change == etp_section.CHANGE_WORSENED_LINK:
			NOT_IMPLEMENTED
                elif self.etp.change == etp_section.CHANGE_IMPROVED_LINK:
			NOT_IMPLEMENTED
		else:
			NOT_IMPLEMENTED

		return etp_interesting

	
	def exec_etp_deadnode(self, me, trcr, tmask):
		"""Returns 1 if the ETP is interesting, 0 otherwise"""
		exist=0
		trash=[]
		
		#e' sempre vero che gw e' l'utimo elemento di trcr?
		#sappiamo solo che e' il nodo che ci ha mandato il pachetto
		
		gw=trcr[len(trcr)-1]
		for r in self.etp.routes:
			
			# this is the route mentioned above formed by
			# the chunks of the TP and that of `r'
			# Its tpmask is  r.tpmask|tmask, and its rem is
			# r.rem+route_of_the_tp.rem
			rp_tracer=[]
			
			for hop in reversed(trcr):
				rp_tracer.append(hop)
			for hop in reversed(r.hops):
				rp_tracer.append(hop)
			
			rp=route(me.nid,r.dst.nid,gw,rp_tracer,self)
			
			# Delete from the map all the routes passing from `gw'
			# and having the tpmask set to `tpm'
			if not me.update_map_deadnode(gw, rp.tpmask):
				# no route has been deleted
				# Try to add r in the map
				if not me.add_route(rp):
				   # this route isn't interesting, we may as
				   # well delete it from the ETP
				   trash.append(r)
		trash=remove_duplicates(trash)
		for r in trash:
			self.etp.routes.remove(r)
		
		if self.etp.routes == []: 
			#the etp is empty!
			
			if self.etp.interest_flag:
				
				self.etp.interest_flag=0

				# The interest_flag was set, but we've found
				# the packet uninteresting, thus we'll send
				# back the new ETP containing our routes
				
				#pack the new ETP
				#if we send back the old ETP it will be infringe
				#the acyclic rule
				#we need to create a new one 
				new_pack=packet(me,G.whole_network[gw],"QSPN")
				link=self.me.rnodes[gw]
				new_pack.add_chunk(link)
				delay=link.l_rem.rtt
				new_pack.time=G.curtime+delay
				
	
				new_etp_routes=[]
				#copy in the new packet all the routes of me
				for dest,marray in me.int_map.iteritems(): 
					new_etp_routes|=self.me.get_all_routes_bydst(G.whole_network[dest])	
					#union of 2 sets 
					#we don't care if it is not sorted
				
				#copy the etp section for the new packet
				new_pack.copy_etp_pkt(self.etp)
				
				#now we have to change the routes of the new pack 
				#with the ones obtained by get_all_routes_bydst
				new_pack.payload.etp.route=new_etp_routes
	
				#send the packet
				new_pack.send_packet()
				
			else:
				# The ETP was considered uninteresting before,
				# thus it is just propagating to carry the
				# information of the death the node
				# `self.etp.changed_node'.
				# Let's check if we already know this info.
				if not me.int_map.has_key(self.etp.changed_node):
					# Yea, we already knew it
					# drop the packet then!
					return 0

		# You may as well forward the ETP
		return 1

	#
	### Tracer Packet 
	#

	def exec_tp(self, me):
		"""Returns 1 if this TP is interesting, 0 otherwise.
		   `me' is the node executing this packet.  """

		whole_tracer=self.create_trcr()
		if G.verbose:
			print "Tracer Pkt:",whole_tracer
		
		G.splitted=[]
		self.split_tracer(me, whole_tracer)
		
		if G.verbose:
			print "G.splitted:",G.splitted
		
		packet_interesting=0
		for i in G.splitted:
			packet_interesting+=self.evaluate_tracer_packet(me, i)
		#	print "packet_interesting:",packet_interesting

		return packet_interesting

	def create_trcr(self):
		trcr=[]
		for i in xrange(len(self.chunks)):
			trcr.append(self.chunks[i].nid)
		return trcr


	def split_tracer(self, me, tp=[]):
		
		"""
		If the TP is in the form:
			XacaY
		split it in two different TPs:
			Xac, caY
		If the TP contains our same hop, split it:
			XmYmZm  ==>  Xm, Ym, Zm
		Where `m' is `me'.
		The routes mY, mZ, will be evaluated as upload routes.
		"""
#		print "to split: ", [o.id for o in tp]

		splitted=0
		l=len(tp)
		for i,k in enumerate(tp):
			if i >= 2 and tp[i] == tp[i-2]:
				# 
				# TODO: For now we just consider the download
				#       routes
				#	This line of code should be enabled
				#	later
				#
				# self.splitted_tracer.append(tp[:i])
				#
#				print "splitting'' (s:%s d:%s): "%(self.src.id, self.dst.id), [x.id for x in tp[:i]]
				self.split_tracer(tp[i-1:])
				splitted=1
				break
			elif i > 0 and i < l-1 and tp[i] == me.nid:
				G.splitted.append(tp[:i+1])
				#
				# TODO: When the upload routes will be
				#       evaluated, the following line should
				#       be: 
				#       self.split_tracer(tp[i:])
				#
#				print "splitting %d (s:%s d:%s): "%(i, self.src.id, self.dst.id), [x.id for x in tp[:i+1]]
				self.split_tracer(tp[i+1:])
				splitted=1
				break
			elif i==0 and tp[i] == me.nid:
				# Skip over this.
				# TODO: enable this for upload routes
				# evaluation
				del tp[i]
				l=len(tp)
				continue

		if splitted == 0:
#			print "splitting' (s:%s d:%s): "%(self.src.id, self.dst.id), [x.id for x in tp]
			G.splitted.append(tp)

	def evaluate_tracer_packet(self, me, tp):
		
		packet_interesting=False
			
		first=True
		for i in reversed(tp):
							
			if first:
				neigh_gw=i
				first=False
				
			if (me.nid!=i):
				#they aren't neighbour!
				#pdb.set_trace()
				r=route(me.nid, i, neigh_gw, tp, self)

				if G.verbose:
					r.print_route()
				# Save the route if it is interesting
				if me.add_route(r):
					packet_interesting=True
			
		return packet_interesting
	

#class packet
class packet:
	
	total_pkts=0
	total_tpkts=0
	
	def __cmp__(self, other):
		return self.time-other.time
	
	def __init__(self,src,dst,pkt_type):	#constructor, used only for the first packet
						#packet type could be qpsn or??
		packet.total_pkts+=1
		self.src=src
		self.dst=dst
		self.me=dst
		self.time=0
		self.payload=tracer_packet(pkt_type)
	
	def print_packet(self):
		print "**********************"
		print "src:",self.src.nid
		print "dst:",self.dst.nid
		print "me:",self.me.nid
		print "time:",self.time
		self.payload.print_trcpkt()
	
	
	def tracer_to_str(self):
		for i in xrange(len(self.payload.chunks)):
			print self.payload.chunks[i].nid,'->'
	
	
	def add_chunk(self,link):	
		self.payload.nchunk+=1
		link_id=link.l_id
		rem_=link.l_rem
		tr_cnk=tracer_chunk(self.src.nid,"QSPN",link_id,rem_,self.src.dp)
		self.payload.chunks.append(tr_cnk)
	
	def send_packet(self):
		G.total_packets_forwarded+=1
		heappush(G.events,self)
		#self.print_packet()
		
	def copy_etp_pkt(self, old_etp):
		self.payload.etp.enabled=old_etp.enabled
		self.payload.etp.change=old_etp.change
		self.payload.etp.change_node=old_etp.change_node
		self.payload.etp.interest_flag=old_etp.interest_flag
		self.payload.etp.route=old_etp.route[:]

	def exec_pkt(self):
		"""self.me has received the `self' packet.
		   Do something with it"""

		if packet.total_pkts % 100==0:
			print "DEBUG stats: tpkts %d, events %d"%(packet.total_pkts,len(G.events))
		tp=self.payload

		# Evaluate the tracer packet, if it isn't interesting, drop
		# it, otherwise forward it to the other rnodes.
		if not tp.etp.enabled:
			# Normal TP
			packet_interesting=tp.exec_tp(self.me)
			self.me.tracer_forwarded+=1
		else:
			# Extended TP
			packet_interesting=tp.exec_etp(self.me)
			self.me.etracer_forwarded+=1

		if G.verbose:
			print "----------------------------"
			print "i'm the node:", self.me.nid
		if not packet_interesting:
			if G.verbose:
				print "completely dropped"
			return 1
		
		if G.verbose:
			print ""

		self.forward_pkt()
		return 0
		
	def forward_pkt(self):
		
		rlen=self.me.num_of_active_neigh()
		if rlen==1:
		   if not self.payload.etp.enabled:
			#We are at the extremity of a segment, reflect back a
			#new TP
			for node,link in self.me.rnodes.iteritems():
				if G.whole_network[node].dead!=1:
					if node!=self.src.nid:
						pack=packet(self.me,G.whole_network[node],"QSPN")
						pack.add_chunk(link)		#add its personal chunk
						
						delay=link.l_rem.rtt
						pack.time=G.curtime+delay
						pack.send_packet()
		   else:
			#The ETPs don't need to be reflected back, because
			#they are acyclic TPs
			return
		else:
			#whole_tracer=self.create_trcr()
			for node, link in self.me.rnodes.iteritems():
				#for all its neighbours 
				# -- each element is a link (neghibour id; (l_id, l_rem))
				if G.whole_network[node].dead != 1:
					if node == self.src.nid:
						continue
					pack=packet(self.me,G.whole_network[node], "QSPN")
					
					#copy all the chunks in the new packet	
					for ch in self.payload.chunks:
						#add its personal chunk
						pack.copy_old_chunks(ch)
					pack.add_chunk(link)

					if self.payload.etp.enabled:
						#copying the ETP section of the packet
						pack.copy_etp_pkt(self.payload.etp)
										
					delay=link.l_rem.rtt
					pack.time=G.curtime+delay
					
					if G.verbose:
						print "forwarding the packet to node: ", node
					
					pack.send_packet()
		#	print "PACCHETTO:"
		#	self.print_packet()
	
	def copy_old_chunks(self,old):
		
		self.payload.nchunk+=1
		tr_cnk=tracer_chunk(old.nid,"QSPN",old.chunk.linkid,old.chunk.rem,old.ndp)
		self.payload.chunks.append(tr_cnk)

#end of class packet		
