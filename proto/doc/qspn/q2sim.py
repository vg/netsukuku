#!/usr/bin/python

from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join

default_rtt=10
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

		if dst.id != src.id:
			delay=self.src.rtt[dst.id]
		else:
			delay=0
		self.time=curtime+delay

		self.tracer.append(dst)

		heappush(events, self)

	def tracer_to_str(self):
		return join([i.id for i in self.tracer], '->')

	def exec_pkt(self):
		#
		# TODO: evaluate the trace of the pkt
		#
		tr=[]
		for i in self.tracer:
			tr.append(i.id)
			myroutes=self.get_routes(i.id)
			### TODO: CONTINUE HERE


		self.me.tracer.append(self.tracer)
		error("src: %s, dst: %s"%(self.src.id, self.dst.id))
		error("\t TP: "+self.tracer_to_str())
		
		# forwards to all rnodes
		for node in self.me.rnode_id:
			nc=graph.graph[node]
			if nc == self.src:
				continue
			p=packet(self.me, nc, self.tracer)

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
		return [o for i in self.route[dst]]
	
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
			error("\t"+self.tracer_to_str())


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
firstpacket=packet(startnode, startnode)
firstpacket.exec_pkt()

while events:
	p=heappop(events)
	curtime=p.time
	p.exec_pkt()

g.dump_tracer_packets()
