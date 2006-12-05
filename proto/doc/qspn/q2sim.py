#!/usr/bin/python
#
#  This file is part of Netsukuku
#  (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
# --
#
# QSPN v2 simulator
# =================
# 
# This is an event-oriented Discrete Event Simulator.
# 
# Each (event,time) pair is pushed in the `G.events' priority queue.
# The main loop of the program will retrieve from the queue the event having the
# lowest `time' value. This "popped" event will be executed.
#
#
# Various tests:
# 	
#	- Complete graph
#	
#	- Mesh graph
# 
#	- Random graphs (or random rtt)
#
# 	- After the first exploration has been completed, change a little the
# 	  graph and explore again. See what happens.
#	
# 	- See what happens if more than one CTPs are sent at the same time
# 	  (from different nodes)
#	
#	- Test with different MAX_ROUTES values
#

from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join
from copy import *
import re
import getopt

class G:
	#
	# Defines
	#
	DEFAULT_RTT	= 100		   # in millisec
	DELTA_RTT	= DEFAULT_RTT/10

	MAX_ROUTES	= 1

	STARTER_NODES	= 1
	EVENTS_LIMIT	= 100000


	#
	# Globals
	#
	curtime=0
	events=[]

	#
	# Flags
	#
	change_graph=False
	verbose=False
	events_limit_reached=False


class packet:
	total_pkts=0
	total_tpkts=0

	def __str__(self):
		return self.tracer_to_str()

	def __cmp__(self, other):
		return self.time-other.time

	def __init__(self, src, dst, trcr):

		packet.total_pkts+=1

		self.src=src
		self.dst=dst
		self.me=dst
		self.tracer=trcr
		self.splitted_tracer=[]
		if not trcr:
			self.first_pkt=1
			packet.total_tpkts+=1
		else:
			self.first_pkt=0

		if dst.id != src.id:
			delay=self.src.rtt[dst.id]
		else:
			delay=0
		self.time=G.curtime+delay

		self.tracer.append(self.me)
		self.split_tracer()

		# put this packet in the event queue
		heappush(G.events, self)

	def tracer_to_str(self):
		return join([i.id for i in self.tracer], '->')

	def split_tracer(self, tp=[]):
		"""
		If the TP is in the form:
			XacaY
		split it in two different TPs:
			Xac, caY
		If the TP contains our same hop, split it:
			XmYmZm  ==>  Xm, Ym, Zm
		Where `m' is `self.me'.
		The routes mY, mZ, will be evaluated as upload routes.
		"""

		if not tp:
			tp=self.tracer[:]
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
			elif i > 0 and i < l-1 and tp[i] == self.me:
				self.splitted_tracer.append(tp[:i+1])
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
			elif i==0 and tp[i] == self.me:
				# Skip over this.
				# TODO: enable this for upload routes
				# evaluation
				del tp[i]
				l=len(tp)
				continue

		if splitted == 0:
#			print "splitting' (s:%s d:%s): "%(self.src.id, self.dst.id), [x.id for x in tp]
			self.splitted_tracer.append(tp)

	def evaluate_tracer_packet(self, tp):
		packet_interesting=False
		tr_old=None
		tr_old_added=False
#		print "Evaluating TP: ", [kk.id for kk in tp]

		skip_hops=-1	# skip just myself
		if len(tp) == 1:
			packet_interesting=True

		for i in reversed(tp[:skip_hops]):
			tr=route(self.me, i, [i])
			if tr_old:
				tr.init_from_routec(tr_old)
				tr.append(i)
				if not tr_old_added:
					del tr_old
			tr_old=tr

			# Save the route if it is interesting
			if self.me.add_route(tr):
				packet_interesting=True
				tr_old_added=True
			else:
				tr_old_added=False

# 		if not packet_interesting:
# 			print "dropped"
# 		else:
# 			print ""
		return packet_interesting

	def forward_pkt(self):
		# forwards to all rnodes

		rlen=len(self.me.rnode_id)
		if rlen == 1:
			# erase the outgoing tracer pkt
			tp=[self.me]
		else:
			tp=self.tracer

		str=[]
		for node in self.me.rnode_id:
			nc=graph.graph[node]
			if nc == self.src and rlen > 1:
				continue
			if nc == self.me:
				EHM_SOMETHING_IS_WRONG
			str.append(node)
			p=packet(self.me, nc, tp[:])
#		print "%s -> {%s}"%(self.me.id, join(str, ","))

	def exec_pkt(self):
		if G.verbose:
			print self.tracer_to_str()+"\t",

		if not self.first_pkt:
			#
			# Evaluate the tracer packet, if it isn't interesting, drop
			# it, otherwise forward it to the other rnodes.
			#
			packet_interesting=0
			for i in self.splitted_tracer:
				packet_interesting+=self.evaluate_tracer_packet(i)

			if not packet_interesting:
				if G.verbose:
					print "completely dropped"
				return 1
		if G.verbose:
			print ""

		# book keeping
		self.me.tracer_forwarded+=1
		#self.me.tracer.append(self.tracer)

		self.forward_pkt()
		return 0
class route:
	def __cmp__(self, other):
		"""Compare two routes.
		   A == B  iif  A has the same hops number of B, the same hops
		                (in order), and the difference of their total rtt is <
		                G.DELTA_RTT.

		   A < B   iif  A has the same hops number of B, the same hops
		   		(in order), and the A.trtt < B.trtt

		   A > B or A != B
		   	   iif  The hops of A differs from those of B or
			        B.trtt > A.trtt
		   
		   Note that "A < B" is returned only in one case
		   (A.trtt < B.trtt). Instead, "A > B or A != B" is ambigous, they 
		   collides with three cases, thus DON'T use A > B. Use A < B
		   instead.
		"""

		if len(self.route) != len(other.route):
			return -1

		for i,k in zip(self.route, other.route):
			if i != k:
				return -1

		if abs(self.trtt-other.trtt) <= G.DELTA_RTT:
			return 0
		else:
			return self.trtt-other.trtt

	def __str__(self):
		return self.route_to_str()

	def __repr__(self):
		return str(self.route)

	def compute_trtt(self):
		if self.route[0].id in self.src.rtt:
			self.trtt=self.src.rtt[self.route[0].id]
		else:
			self.trtt=0
		for i,r in enumerate(self.route[:-1]):
			self.trtt+=r.rtt[self.route[i+1].id]

	def compute_links_id(self):
		self.links_id={}
		iid=self.src.id+'->'+self.route[0].id
		if self.route[0].id in self.src.rtt and iid in self.src.link_id and self.src.link_id[iid] > 0:
#			print "%s :="%iid, self.src.link_id[iid]
			self.links_id[iid]=self.src.link_id[iid]
			iid=self.route[0].id+'->'+self.src.id
			self.links_id[iid]=self.src.link_id[iid]

		for i,r in enumerate(self.route[:-1]):
			iid=r.id+'->'+self.route[i+1].id
			if r.link_id[iid] > 0:
#				print "%s +="%iid, r.link_id[iid]
				self.links_id[iid]=r.link_id[iid]
				iid=self.route[i+1].id+'->'+r.id
				self.links_id[iid]=r.link_id[iid]

	def __init__(self, src, dst, route):
		self.src=src
		self.dst=dst
		self.route=route
		self.links_id={}
		self.compute_links_id()
		self.compute_trtt()

	def init_from_routec(self, routec):
		self.dst=routec.dst
		self.route=routec.route[:]
		self.links_id=routec.links_id
		self.trtt=routec.trtt

	def append(self, node):
		self.dst=node
		if not self.route:
			self.route=[node]
			self.compute_trtt()
			self.compute_links_id()
		else:
			self.route.append(node)
			self.trtt+=self.route[-2].rtt[node.id]

			nid=self.route[-2].id+'->'+node.id
			if nid in self.route[-2].link_id and  self.route[-2].link_id[nid] > 0:
				iid=self.route[-2].id+'->'+node.id
				self.links_id[iid]=self.route[-2].link_id[nid]
				iid=node.id+'->'+self.route[-2].id
				self.links_id[iid]=self.route[-2].link_id[nid]

	def extend(self, dst, route):
		self.dst=dst
		self.route.extend(route)
		self.compute_trtt()

	def route_to_str(self):
		return join([i.id for i in self.route], '->')

class node:
	total_nodes=0
	
	def __hash__(self):
		return hash(self.id)

	def __str__(self):
		return self.me.id

	def __init__(self, id, rnodes):
		self.rtt={}
		self.route={}
		self.rnode_id=[]
		self.link_id={}
		self.link_id_rcv={}
		self.id=id
		self.tracer=[]
		self.tracer_forwarded=0
		self.worst_route={}

		for (node, rtt) in rnodes:
			self.rtt[node]=rtt
			self.rnode_id.append(node)

	def tracer_to_str(self):
		ttp=[join([o.id for o in i], '->') for i in self.tracer]
		return join(ttp, '\n\t')

	def get_routes(self, dst):
		if dst not in self.route:
			return []
		else:
			return self.route[dst]

	def add_route(self, route):
		interesting=True
		dst=route.dst.id

		if dst not in self.route:
			self.route[dst]=[route]
			self.worst_route[dst]=0
		else:
			rarray=[k.route for k in self.route[dst]]
#			print self.id,
#			print route, [k.route for k in self.route[dst]]
			if route.route in rarray:
				ridx=rarray.index(route.route)
				
				if route < self.route[dst][ridx] or\
						self.update_link_id(route):
					ptr=self.route[dst][ridx]
					self.route[dst][ridx]=route
					del ptr
					if ridx == self.worst_route[dst]:
						self.compute_worst_route(dst)
				else:
					# This route has been already added
					# and it's not interesting
					return not interesting
			else:
				# Add the route
				self.route[dst].append(route)

#		print "%s -> %s: %d"%(self.id, dst, route.trtt)
		if self.route[dst][self.worst_route[dst]] < route:
			# The added route is the worst
			self.worst_route[dst]=self.route[dst].index(route)

			if len(self.route[dst]) > G.MAX_ROUTES:
				# The maximum number of stored routes has been
				# reached. Drop the worst route.
				self.purge_worst_route(dst)
				return not interesting

		# Clean the house
		if len(self.route[dst]) > G.MAX_ROUTES:
			self.purge_worst_route(dst)

		# The added route is not the worst, it's interesting
		return interesting

	def update_link_id(self, route):
		new_link_id=False
		rt=[self]+route.route
		dst=route.dst.id

#		print "update_link_id: ", [oo.id for oo in rt],
#		print "self.link_id: ", self.link_id

		for i,r in enumerate(rt[:-1]):
			newid=False
			iid=rt[i].id+'->'+rt[i+1].id
			iid2=rt[i+1].id+'->'+rt[i].id
#			print "update_link_id iid: ", iid

			if iid not in route.links_id:
#				print "%s is not in the route"%iid
				continue

			if (iid not in self.link_id) or\
					(self.link_id[iid] < route.links_id[iid]) or\
					(self.link_id[iid] == route.links_id[iid] and\
					self.link_id_rcv[iid] < self.link_id[iid]):
#				print "%s hadn't the link id %s"%(self, iid)
				self.link_id[iid]=route.links_id[iid]
				self.link_id[iid2]=route.links_id[iid2]
				if self.link_id[iid] > 0:
					new_link_id=newid=True
					self.link_id_rcv[iid]=self.link_id[iid]
			elif self.link_id[iid] > route.links_id[iid]:
				print "Invalid link id %s: %d, cur: %d"%(iid, route.links_id[iid], self.link_id[iid])
				
			if newid:
				if G.verbose:
					print "new link_id %s: %d"%(iid, self.link_id[iid]),
#			else:
#				print "normal link: ",iid

		return new_link_id

	def purge_worst_route(self, dst):
		del self.route[dst][self.worst_route[dst]]
		self.compute_worst_route(dst)

	def compute_worst_route(self, dst):
		# update the worst route
		worst=self.route[dst][0]
		for i in self.route[dst]:
			if worst < i:
				worst=i
		self.worst_route[dst]=self.route[dst].index(worst)

	def dump_routes(self, dst):
		if dst not in self.route:
			return "None"
		else:
			return join([i.route_to_str()+' ('+str(i.trtt)+')' for i in self.route[dst]], '||')

class graph:
	graph={}
	graph_len=0

	def load_graph(self, file):
		"""The format of the file must be:
			
			nodeX -- nodeY -- weightXY -- weightYX
			nodeX -- nodeZ -- weightXZ -- weightZX
			...
		   The "-- weightXY -- weightYX" part is optional.
		   All the lines of the file which aren't in this form are
		   ignored. In other words, you can use Graphviz (.dot) file.
		"""
		nodes={}
		
		f=open(file)
		for l in f:
			w=re.split(' *-- *', l.strip())
			if len(w) == 1:
				continue

			if len(w) < 3:
				weightXY=G.DEFAULT_RTT
			else:
				weightXY=w[2]
			if len(w) < 4:
				weightYX=weightXY
			else:
				weightYX=w[3]

			if w[0] not in nodes:
				nodes[w[0]]=[]
			if w[1] not in nodes:
				nodes[w[1]]=[]
			nodes[w[0]].append((w[1], weightXY))
			nodes[w[1]].append((w[0], weightYX))

		for id,rnodes in nodes.iteritems():
			graph.graph[id]=node(id, rnodes)
		graph.graph_len=len(graph.graph)

	def gen_complete_graph(self, k):
		k+=1
		for i in xrange(1,k):
			nstr='n%d'%i
			rnodes=[('n%d'%o, G.DEFAULT_RTT) for o in xrange(1, k) if o != i]
			n=node(nstr, rnodes)
			graph.graph[nstr]=n

		graph.graph_len=len(graph.graph)

	def gen_mesh_graph(self, k):
		for x in xrange(k):
			for y in xrange(k):
				nstr='n%d'%(x*k+y)
				rnodes=[]

				if x > 0:
					#left
					rnodes.append(('n%d'%((x-1)*k+y), G.DEFAULT_RTT))
				if x < k-1:
					#right
					rnodes.append(('n%d'%((x+1)*k+y), G.DEFAULT_RTT))
				if y > 0:
					#down
					rnodes.append(('n%d'%(x*k+(y-1)), G.DEFAULT_RTT))
				if y < k-1:
					#up
					rnodes.append(('n%d'%(x*k+(y+1)), G.DEFAULT_RTT))

				n=node(nstr, rnodes)
				graph.graph[nstr]=n

		graph.graph_len=len(graph.graph)

	def gen_rand_graph(self, k):
		nodes={}
		rtt={}
		if k < 2:
			print "The number of nodes for the random graph must be >= 2"
			sys.exit(2)

		for i in xrange(k):
			if i not in nodes:
				nodes[i]=[]
				rtt[i]=[]
			r=random.randint(1, max(k/4, 1))
			for xx in xrange(r):
				ms=random.randint(G.DELTA_RTT, G.DEFAULT_RTT*2)

				# Choose a random rnode which is not  i  and
				# which hasn't been already choosen
				rn=range(k)
				random.shuffle(rn)
				while rn[0] == i or rn[0] in nodes[i]:
					if len(rn) == 1:
						break
					else:
						del rn[0]
				if rn[0] == i or rn[0] in nodes[i]:
					continue
				rn=rn[0]

				if rn not in nodes:
					nodes[rn]=[]
					rtt[rn]=[]

				nodes[i].append(rn)
				rtt[i].append(ms)
				nodes[rn].append(i)
				rtt[rn].append(ms)

		for nid, rnodes in nodes.iteritems():
			n=node('n%d'%nid, zip(['n%d'%o for o in rnodes], rtt[nid]))
			graph.graph['n%d'%nid]=n

		graph.graph_len=len(graph.graph)

	def print_dot(self, file):
		f=open(file, 'w')
		f.write("graph G {\n")
		f.write("\tnode [shape=circle]\n\n")
		for id, node in graph.graph.iteritems():
			for l in node.rnode_id:
				f.write("\t"+id+" -- "+l+"   [weight=%d]"%node.rtt[l]+"\n")
		f.write("}\n")
		f.close()
	
	def dump_tracer_packets(self):
		for id, node in graph.graph.iteritems():
			print "Node "+id
			print "\t"+node.tracer_to_str()

	def dump_stats(self):
		# compute the Mean Flux of TPs
		mean=n=0.0
		missing_routes=0
		if G.verbose:
			print "Routes table:"
		for id, node in graph.graph.iteritems():
			mean+=node.tracer_forwarded
			n+=1
			for id2, node2 in graph.graph.iteritems():
				if id == id2:
					continue

				if id2 not in node.route:
					print "\tMissing route: %s -> %s"%(id, id2)
					missing_routes+=1
				elif G.verbose:
					print "\t%s  to  %s  via "%(id, id2), node.dump_routes(id2)
		mean=mean/n
		print "Total nodes:\t", len(graph.graph)
		print "Total packets:\t", packet.total_pkts
		print "Individual TPs:\t", packet.total_tpkts
		print "Mean TPs flux:\t", mean
		if G.verbose:
			print "Single TPs flux:\t"
			for id, node in graph.graph.iteritems():
				print "\t%s  %d"%(id,node.tracer_forwarded)
		print "Missing routes:\t", missing_routes
		print "Total time:\t", G.curtime/1000.0, "sec"

	def reset_stats(self):
		for id, node in graph.graph.iteritems():
			node.tracer_forwarded=0

		packet.total_pkts=packet.total_tpkts=0
		G.curtime=0

def change_graph(nlinks):
	for i in xrange(nlinks):
		n=graph.graph[random.choice(graph.graph.keys())]
		l=random.choice(n.rnode_id)

		# rtt(n --> rn) = rtt(rn --> n) = rand
		rold=n.rtt[l]
		n.rtt[l]=random.randint(G.DELTA_RTT, G.DEFAULT_RTT*2)
		rn=graph.graph[l]
		rn.rtt[n.id]=n.rtt[l]

		# Update the link id
		print "\tchanging",n.id+'->'+rn.id, 'rtt: %d -> %d'%(rold, n.rtt[l])

		if n.id+'->'+rn.id not in rn.link_id:
			rn.link_id[n.id+'->'+rn.id]=1
			rn.link_id_rcv[n.id+'->'+rn.id]=0
			rn.link_id[rn.id+'->'+n.id]=1
			rn.link_id_rcv[rn.id+'->'+n.id]=0
		else:
			rn.link_id[n.id+'->'+rn.id]+=1
			rn.link_id[rn.id+'->'+n.id]+=1
		if n.id+'->'+rn.id not in n.link_id:
			n.link_id[n.id+'->'+rn.id]=1
			n.link_id_rcv[n.id+'->'+rn.id]=0
			n.link_id[rn.id+'->'+n.id]=1
			n.link_id_rcv[rn.id+'->'+n.id]=0
		else:
			n.link_id[n.id+'->'+rn.id]+=1
			n.link_id[rn.id+'->'+n.id]+=1

		# TP(n --> rn)
		packet(n, rn, [n])
		# TP(rn --> n)
		packet(rn, n, [rn])

		packet.total_tpkts+=2

#
# Main loop
# 

def main_loop():
	idx=1
	while G.events:
		if idx==G.EVENTS_LIMIT:
			print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
			print "-----------[[[[ ===== --- BREAK --- ===== ]]]]----------------"
			print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
			G.events_limit_reached=True
			break
		p=heappop(G.events)
		G.curtime=p.time
		if p.exec_pkt():
			# The packet has been dropped
			del p
		idx+=1


def start_exploration(starters=[]):
	if not starters:
		# Randomly chosen starters
		if G.STARTER_NODES <= 0:
			print "[!] The number of starter nodes must be > 0,"
			print "    where g is the number of nodes of the graph."
			sys.exit(2)

		for k in xrange(G.STARTER_NODES):
			startnode=graph.graph[random.choice(graph.graph.keys())]
			starters.append(startnode)
	
	for i in starters:
		packet(i, i, [])
	print "[*] Starting node %s"%(join([i.id for i in starters], ','))


def usage():
	print "Usage:"
	print "\tq2sim.py [-pvh] [-g graph.dot] [-o out.dot]"
	print ""
	print "\t-h\t\tthis help"
	print "\t-v\t\tverbose"
	print "\t-p\t\tenable psyco optimization (see http://psyco.sourceforge.net/)"
	print ""
	print "\t-k [n]\t\tuse a complete graph of n nodes (default n=8)"
	print "\t-m [n]\t\tuse a mesh graph of n*n nodes (default n=4)"
	print "\t-r [n]\t\tuse a random graph of n nodes (default n=8)"
	print "\t-g file\t\tload the graph from file"
	print "\t-o file\t\tspecify the output graph file (default is outgraph.dot)"
	print ""
	print "\t-l n\t\tSet the maximum number of events to n (defaul n=%d)"%G.EVENTS_LIMIT
	print "\t-R n\t\tMaximum number of routes kept in the QSPN cache (defaul n=%d)"%G.MAX_ROUTES
	print "\t-s n\t\tSet to n the number of starter nodes (default n=%d"%G.STARTER_NODES
	print "\t-c n\t\tWhen the graph exploration terminates, modifies n links"
	print "\t    \t\tand let the interestered nodes send a CTP. By default"
	print "\t    \t\tthis is disabled "
	print ""
	print "\t-S s\t\tUse  s  as the seed for the pseudrandom generator"


#
# main()
#

def main():
	shortopt="hpvo:g:l:k::m::r::s:R:c:S:"

	try:
		opts, args = getopt.gnu_getopt(sys.argv[1:], shortopt, ["help"])
	except getopt.GetoptError:
		usage()
		sys.exit(2)

	outgraph="outgraph.dot"
	kgraph=rgraph=8
	mgraph=4
	ingraph=""
	G.seed=random.randint(0, 0xffffffff)
	random.seed(G.seed)

	for o, a in opts:
		if o == "-v":
			G.verbose = True
		if o in ("-h", "--help"):
			usage()
			sys.exit()
		if o == "-p":
			print "Enabling psyco optimization"
			import psyco
			psyco.full()
		if o == "-o":
			outgraph = a
		if o == "-g":
			ingraph = a
		if o == "-k":
			kgraph = int(a)
			if kgraph == 0:
				kgraph = 8
			mgraph=rgraph=0
		if o == "-m":
			mgraph = int(a)
			if mgraph == 0:
				mgraph = 8
			kgraph=rgraph=0
		if o == "-r":
			rgraph=int(a)
			if rgraph == 0:
				rgraph = 8
			kgraph=mgraph=0
		if o == "-l":
			G.EVENTS_LIMIT=int(a)
		if o == "-s":
			G.STARTER_NODES=int(a)
		if o == "-R":
			G.MAX_ROUTES=int(a)
		if o == "-c":
			G.change_graph=int(a)
		if o == "-S":
			G.seed=int(a)
			random.seed(G.seed)
			
	print join(sys.argv+['-S %d'%G.seed], ' ')
	g=graph()

	if ingraph:
		print "[*] Loading graph from "+ingraph
		g.load_graph(ingraph)
	elif mgraph > 0:
		print "[*] Generating a mesh graph of %d*%d=%d node"%(mgraph,mgraph, mgraph*mgraph)
		g.gen_mesh_graph(mgraph)
	elif kgraph > 0:
		print "[*] Generating a complete graph of %d node"%kgraph
		g.gen_complete_graph(kgraph)
	elif rgraph > 0:
		print "[*] Generating a random graph of %d node"%rgraph
		g.gen_rand_graph(rgraph)

	print "[*] Saving the graph to "+outgraph
	g.print_dot(outgraph)

	# The main loop begins
	start_exploration()
	main_loop()


	if G.change_graph:
		print "\n---- Statistics ----"
		g.dump_stats()
		print "[*] Resetting the statistics"
		g.reset_stats()
		print "[*] Modifying %d links of the graph"%G.change_graph
		change_graph(G.change_graph)
		print "[*] Starting the new exploration"
		main_loop()


	if G.events_limit_reached:
		print "[!] Warning: The simulation has been aborted, because the"
		print "             events limit (%d) has been reached"%G.EVENTS_LIMIT

	print "\n---- Statistics ----"
	g.dump_stats()

	
if __name__ == "__main__":
    main()
