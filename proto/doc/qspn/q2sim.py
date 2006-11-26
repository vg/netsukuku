#!/usr/bin/python

from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join

default_rtt=100
delta_rtt=default_rtt/10
curtime=0
events=[]

class packet:
	total_pkts=0

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
		if not tp:
			tp=self.tracer

		splitted=0
		for i,k in enumerate(tp):
			if i >= 2 and tp[i] == tp[i-2]:
				self.splitted_tracer.append(tp[:i])
				self.split_tracer(tp[i-1:])
				splitted=1
				break
		if splitted == 0:
			self.splitted_tracer.append(tp)

	def evaluate_tracer_packet(self, tp):
		packet_interesting=0
		tr=route()
		old=""
		for i in reversed(tp):
			if i == self.me:
				continue
			if i == old:
				AAAAAAAAAAAAAAAAAAAAAAAAAAAA
				## TODO: continue here
			old=i
			tr.append(i)

			iroutes=self.me.get_routes(i.id)

			# Save the route if it is interesting
			if (tr in iroutes and tr < iroutes[iroutes.index(tr)]) or (tr not in iroutes):
					self.me.route[i.id].append(tr)
					packet_interesting=1

		return packet_interesting

	def forward_pkt(self):
		# forwards to all rnodes

		rlen=len(self.me.rnode_id)
		if rlen == 1:
			# erase the outgoing tracer pkt
			tp=[self.me]
		else:
			tp=self.tracer

		for node in self.me.rnode_id:
			nc=graph.graph[node]
			if nc == self.src and rlen > 1:
				continue
			p=packet(self.me, nc, tp)

	def exec_pkt(self):
		if not self.first_pkt:
			#
			# Evaluate the tracer packet, if it isn't interesting, drop
			# it, otherwise forward it to the other rnodes.
			#
			packet_interesting=0
			for i in self.splitted_tracer:
				packet_interesting+=self.evaluate_tracer_packet(i)

			if packet_interesting == 0:
				return

		# book keeping
		self.me.tracer.append(self.tracer)
		error("src: %s, dst: %s"%(self.src.id, self.dst.id))
		error("\t TP: "+self.tracer_to_str())

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
		return join(ttp, '::')

	def get_routes(self, dst):
		return [i for i in self.route[dst]]


class graph:
	graph={}

	def load_graph(self, file):
		"""The format of the file must be:
			
			nodeX--nodeY--weight1
			nodeX--nodeZ--weight2
			...
		"""
		nodes=[]
		
		f=open(file)
		for l in f:
			w=l.split('--')
			nodes[w[0]].append([w[1], w[2]])
			nodes[w[1]].append([w[0], w[2]])

		for i in nodes.iterkeys():
			n=node(i, nodes[i])
			graph.graph[i]=n

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


def error(str):
	stderr.write(str+"\n")

#
# main()
#

g=graph()
#g.load_graph(sys.argv[1])
g.gen_complete_graph(4)
g.print_dot("test.dot")
#sys.exit(0)

startnode=graph.graph[random.choice(graph.graph.keys())]
first_packet=packet(startnode, startnode)
first_packet.exec_pkt()

while events:
	p=heappop(events)
	curtime=p.time
	p.exec_pkt()

g.dump_tracer_packets()
