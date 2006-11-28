#!/usr/bin/python

from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join
from copy import *

default_rtt=100
delta_rtt=default_rtt/10
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
		print "Evaluating TP: ", [i.id for i in tp],
		for i in reversed(tp[:-1]):
			tr=route()
			if tr_old:
				tr.init_from_routec(tr_old)
			tr_old=tr
			tr.append(i)

			iroutes=self.me.get_routes(i.id)

			# Save the route if it is interesting
			if (tr not in iroutes):
				#TODO: or (tr < iroutes[iroutes.index(tr)]):
					if i.id not in self.me.route:
						self.me.route[i.id]=[tr]
					else:
						self.me.route[i.id].append(tr)
					packet_interesting=1
		if packet_interesting == 0:
			print "dropped"
		else:
			print ""
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
			str.append(node)
			p=packet(self.me, nc, tp[:])
		print "%s -> {%s}"%(self.me.id, join(str, ","))

	def exec_pkt(self):
		error("src: %s, dst: %s"%(self.src.id, self.dst.id))
		error("\t TP: "+self.tracer_to_str())
		if not self.first_pkt:
			#
			# Evaluate the tracer packet, if it isn't interesting, drop
			# it, otherwise forward it to the other rnodes.
			#
			packet_interesting=0
			for i in self.splitted_tracer:
				packet_interesting+=self.evaluate_tracer_packet(i)

			if packet_interesting == 0:
				print "completely dropped"
				return

		# book keeping
		self.me.tracer.append(self.tracer)

		self.forward_pkt()

class route:
	def __cmp__(self, other):
		if len(self.route) != len(other.route):
			return -1

		for i,k in zip(self.route, other.route):
			if i != k:
				return -1

		if abs(self.trtt-other.trtt) <= delta_rtt:
			return 0
		else:
			return self.trtt-other.trtt

	def compute_trtt(self):
		self.trtt=0
		for i,r in enumerate(self.route[:-1]):
			self.trtt+=r.rtt[self.route[i+1].id]

	def __init__(self, dst=None, route=[]):
		self.dst=dst
		self.route=route
		self.compute_trtt()

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

	def init_from_routec(self, routec):
		self.dst=routec.dst
		self.route=routec.route[:]
		self.trtt=routec.trtt

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
		self.rcv_pkt=[]
		self.tracer=[]

		for (node, rtt) in rnodes:
			self.rtt[node]=rtt
			self.route[node]=[]
			self.rnode_id.append(node)

	def tracer_to_str(self):
		ttp=[join([o.id for o in i], '->') for i in self.tracer]
		return join(ttp, '\n\t')

	def get_routes(self, dst):
		if dst not in self.route:
			return []
		else:
			return self.route[dst]

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
				weight=default_rtt
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
			n=node('n%d'%i, [('n%d'%o, default_rtt) for o in range(1, k) if o != i])
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
			mean+=len(node.tracer)
			n+=1
		mean=mean/n
		print "Total packets:\t", packet.total_pkts
		print "Individual TPs:\t", packet.total_tpkts
		print "Mean TPs flux:\t", mean

def error(str):
	stderr.write(str+"\n")

#
# main()
#

random.seed(12)

g=graph()
if len(sys.argv) > 1:
	g.load_graph(sys.argv[1])
else:
	g.gen_complete_graph(4)
g.print_dot("test.dot")
#sys.exit(0)

startnode=graph.graph[random.choice(graph.graph.keys())]
first_packet=packet(startnode, startnode)

idx=0
while events:
	if idx==1000:
		print "[[[[ ===== --- HELL --- ===== ]]]]"
		break
	p=heappop(events)
	curtime=p.time
	p.exec_pkt()
	idx+=1

g.dump_tracer_packets()
g.dump_stats()
