#!/usr/bin/python

from heapq import *
import random
import sys

default_rtt=10
curtime=0
events=[]

class packet:

	total_pkts=0

	def __cmp__(self, other):
		return self.time-other.time

	def __init__(self, src, dst):
		global curtime

		total_pkts+=1

		self.src=src
		self.dst=dst
		self.me=dst

		delay=self.src.rtt[dst]
		self.time=curtime+delay

		self.tracer.append(dst)

		heappush(events, self)

	def exec_pkt(self):
		#
		# TODO: evaluate the trace of the pkt
		#
		
		# forwards to all rnodes
		for (node, rtt) in self.me.rnode_id:
			nc=graph.graph[node]
			if nc == self.src:
				continue
			p=packet(self.me, nc)

class node:
	total_nodes=0
	
	def __hash__(self):
		return self.id

	def __init__(self, id, rnodes):
		self.rtt={}
		self.rnode_id=[]
		self.id=id
		self.rcv_pkt=[]

		for (node, rtt) in rnodes:
			self.rtt[node]=rtt
			self.rnode_id.append(node)

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
			n=node('n%d'%i, [('n%d'%o, default_rtt) for o in range(i+1, k)])
			graph.graph['n%d'%i]=n
	
	def print_dot(self):
		print "graph G {"
		print "\tnode [shape=circle]"
		print ""
		for id, node in graph.graph.iteritems():
			for l in node.rnode_id:
				print "\t"+id+" -- "+l+" [weight=%d]"%node.rtt[l]
		print "}"

#
# main()
#

g=graph()
#g.load_graph(sys.argv[1])
g.gen_complete_graph(4)
g.print_dot()
#####################
sys.exit(0)
#####################

startnode=graph[random.choice(graph.keys())]
firstpacket=packet(startnode, startnode)

while events:
	p=heappop(events)
	curtime=p.time
	p.exec_pkt()
