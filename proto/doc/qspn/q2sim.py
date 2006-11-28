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
# Each (event,time) pair is pushed in the `events' priority queue.
# The main loop of the program will retrieve from the queue the event having the
# lowest `time' value. This "popped" event will be executed.
#
#

# TODO: Various tests:
# 	
# 	- See what happens if more than one CTPs are sent at the same time
# 	  (from different nodes)
# 
#	- Random graphs
#
#	- Random rtt
#

from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join
from copy import *

#
# Defines
#
DEFAULT_RTT	=100
DELTA_RTT	=DEFAULT_RTT/10

MAX_ROUTES	= 1

EVENTS_LIMIT	= 100000

#
# Globals
#
curtime=0
events=[]

class packet:
	total_pkts=0
	total_tpkts=0

	def __cmp__(self, other):
		return self.time-other.time

	def __init__(self, src, dst, tracer=[]):
		global curtime

		packet.total_pkts+=1

		self.src=src
		self.dst=dst
		self.me=dst
		self.tracer=tracer
		self.splitted_tracer=[]
		if not tracer:
			self.first_pkt=1
			packet.total_tpkts+=1
		else:
			self.first_pkt=0

		if dst.id != src.id:
			delay=self.src.rtt[dst.id]
		else:
			delay=0
		self.time=curtime+delay

		self.tracer.append(self.me)
		self.split_tracer()

		# put this packet in the event queue
		heappush(events, self)

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
		packet_interesting=0
		tr_old=None
		tr_old_added=False
#		print "Evaluating TP: ", [i.id for i in tp],
		for i in reversed(tp[:-1]):
			tr=route()
			if tr_old:
				tr.init_from_routec(tr_old)
				if not tr_old_added:
					del tr_old
			tr_old=tr
			tr.append(i)

			# Save the route if it is interesting
			if self.me.add_route(tr):
				packet_interesting=1
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
		# DEBUG
		print self.tracer_to_str(),
		if not self.first_pkt:
			#
			# Evaluate the tracer packet, if it isn't interesting, drop
			# it, otherwise forward it to the other rnodes.
			#
			packet_interesting=0
			for i in self.splitted_tracer:
				packet_interesting+=self.evaluate_tracer_packet(i)

			if not packet_interesting:
				print "  completely dropped"
				return 1
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
		                DELTA_RTT.

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

		if abs(self.trtt-other.trtt) <= DELTA_RTT:
			return 0
		else:
			return self.trtt-other.trtt

	def __str__(self):
		return self.route_to_str()

	def __repr__(self):
		return str(self.route)

	def compute_trtt(self):
		self.trtt=0
		for i,r in enumerate(self.route[:-1]):
			self.trtt+=r.rtt[self.route[i+1].id]

	def __init__(self, dst=None, route=[]):
		self.dst=dst
		self.route=route
		self.compute_trtt()

	def init_from_routec(self, routec):
		self.dst=routec.dst
		self.route=routec.route[:]
		self.trtt=routec.trtt

	def append(self, node):
		self.dst=node
		if not self.route:
			self.route=[node]
			self.trtt=0
		else:
			self.route.append(node)
			self.trtt+=self.route[-2].rtt[node.id]

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

	def __init__(self, id, rnodes):
		self.rtt={}
		self.route={}
		self.rnode_id=[]
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
			if route in self.route[dst]:
				# The route has been already added
				return not interesting
			else:
				# Add the route
				self.route[dst].append(route)

#		print "%s -> %s: %d"%(self.id, dst, route.trtt)
		if self.route[dst][self.worst_route[dst]] < route:
			# The added route is the worst
			self.worst_route[dst]=self.route[dst].index(route)

			if len(self.route[dst]) > MAX_ROUTES:
				# The maximum number of stored routes has been
				# reached. Drop the worst route.
				self.purge_worst_route(dst)
				return not interesting

		# Clean the house
		if len(self.route[dst]) > MAX_ROUTES:
			self.purge_worst_route(dst)

		# The added route is not the worst, it's interesting
		return interesting

	def purge_worst_route(self, dst):
		del self.route[dst][self.worst_route[dst]]

		# update the worst route
		worst=self.route[dst][0]
		for i in self.route[dst]:
			if worst < i:
				worst=i
		self.worst_route[dst]=self.route[dst].index(worst)

class graph:
	graph={}

	def load_graph(self, file):
		"""The format of the file must be:
			
			nodeX--nodeY--weight1
			nodeX--nodeZ--weight2
			...
		   The "--weight1" part is optional.
		"""
		nodes={}
		
		f=open(file)
		for l in f:
			w=l.strip().split('--')
			if len(w) < 3:
				weight=DEFAULT_RTT
			else:
				weight=w[2]
			if w[0] not in nodes:
				nodes[w[0]]=[]
			if w[1] not in nodes:
				nodes[w[1]]=[]
			nodes[w[0]].append((w[1], weight))
			nodes[w[1]].append((w[0], weight))

		for id,rnodes in nodes.iteritems():
			graph.graph[id]=node(id, rnodes)

	def gen_complete_graph(self, k):
		k+=1
		for i in range(1,k):
			n=node('n%d'%i, [('n%d'%o, DEFAULT_RTT) for o in range(1, k) if o != i])
			graph.graph['n%d'%i]=n
	
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
			error("Node "+id)
			error("\t"+node.tracer_to_str())

	def dump_stats(self):
		# compute the Mean Flux of TPs
		mean=n=0.0
		for id, node in graph.graph.iteritems():
			mean+=node.tracer_forwarded
			n+=1
		mean=mean/n
		print "Total nodes:\t", len(graph.graph)
		print "Total packets:\t", packet.total_pkts
		print "Individual TPs:\t", packet.total_tpkts
		print "Mean TPs flux:\t", mean

def error(str):
	stderr.write(str+"\n")

#
# main()
#

random.seed(12)

import psyco
psyco.full()
 

g=graph()
if len(sys.argv) == 2:
	g.load_graph(sys.argv[1])
elif len(sys.argv) == 3:
	g.gen_complete_graph(int(sys.argv[1]))
else:
	g.gen_complete_graph(4)
g.print_dot("test.dot")
#sys.exit(0)

startnode=graph.graph[random.choice(graph.graph.keys())]
first_packet=packet(startnode, startnode)

idx=0
while events:
	if idx==EVENTS_LIMIT:
		print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
		print "-----------[[[[ ===== --- BREAK --- ===== ]]]]----------------"
		print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
		break
	p=heappop(events)
	curtime=p.time
	if p.exec_pkt():
		# The packet has been dropped
		del p
	idx+=1

#g.dump_tracer_packets()
print "\n---- Statistics ----"
g.dump_stats()

if idx==EVENTS_LIMIT:
	print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
	print "-----------[[[[ ===== --- BREAK --- ===== ]]]]----------------"
	print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
