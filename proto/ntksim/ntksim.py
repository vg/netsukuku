#!/usr/bin/python
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
# --
#
# Netsukuku simulator
# ===================
# 
# This is an event-oriented Discrete Event Simulator.
# 
# Each (event,time) pair is pushed in the `G.events' priority queue.
# The main loop of the program will retrieve from the queue the event having the
# lowest `time' value. This "popped" event will be executed.
#


from heapq import *
import random
import sys
from sys import stdout, stderr
from string import join
from copy import *
from random import Random
import re
import getopt
import string
import pdb
import math
from G import *
from node import *
from maps import *
from packet import *
from route import *
from rem import *

class graph:
	
	#in this class I have the map of the whole phisical network
		
	def print_graph_len(self):
		print "graph_len:", len(G.whole_network)
		
		
	def print_graph(self):
		nkeys=G.whole_network.keys()
		nkeys.sort()
		for node_keys in nkeys:
			G.whole_network[node_keys].print_node()
		#	self.whole_network[node_keys].get_level_id(3)
		
	def load_graph(self, file):
		
		"""The format of the file must be:
			
			nodeX -- nodeY -- rtt -- bwXY -- bwYX
			nodeX -- nodeZ -- rtt -- bwXZ -- bwZX
			...
		   The "-- weightXY -- weightYX" part is optional.
		   All the lines of the file which aren't in this form are
		   ignored. In other words, you can use Graphviz (.dot) file.
		"""
		
		f=open(file)
		for l in f:
			w=re.split(' *-- *', l.strip())
			if len(w) == 1:
				continue
			
			if len(w)==5: 
			         rtt=float(w[2])
				 bwXY=float(w[3])
				 bwYX=float(w[4])
			
			elif len(w) == 2:
				rtt=G.DEFAULT_RTT
				bwXY=G.DEFAULT_BW
				bwYX=G.DEFAULT_BW
			else:
				print "ERROR: The format of the file must be: \n nodeX -- nodeY -- rtt -- bwXY -- bwYX \n nodeX -- nodeZ -- rtt -- bwXZ -- bwZX \n ..."
				sys.exit()
			
			first_n=int(w[0])
			second_n=int(w[1])
			
			#pdb.set_trace()
			rem_first=rem_t(rtt,bwXY,bwYX,0)	#create a rem per each direction of the link; the dp will assigned later per each node
			rem_second=rem_t(rtt,bwYX,bwXY,0)	#create a rem per each direction of the link; the dp will assigned later per each node

			if G.whole_network.has_key(first_n):	#the nodeX already exist
				G.whole_network[first_n].new_link(second_n,rem_first) #add to nodeX the link for the nodeY
			else:
				nodex = node(first_n, second_n,rem_first) #create an istance of the nodeX 
				
				if G.DP_ENHANCEMENT:	#assign a death probability
					unif=random.uniform(0,1)
					nodex.dp=-1/G.LAMBDA*math.log(1-unif*(1-math.exp(-G.LAMBDA*0.1)))
				
				G.whole_network[first_n]=nodex
				nodex.hook()
				

		
			if G.whole_network.has_key(second_n):	#the nodeY already exist
				G.whole_network[second_n].new_link(first_n,rem_second) #add to nodeY the link for the nodeX
			else:
				nodey = node(second_n, first_n,rem_second) #create an istance of the nodex 
				
				if G.DP_ENHANCEMENT:	#assign a death probability
					unif=random.uniform(0,1)
					nodey.dp=-1/G.LAMBDA*math.log(1-unif*(1-math.exp(-G.LAMBDA*0.1)))
				
				G.whole_network[second_n]=nodey
				nodey.hook()


###
##### MAIN CODE
###

def start_exploration(starters=[]):
	if not starters:
		# Randomly chosen starters
		if G.STARTER_NODES <= 0:
			print "[!] The number of starter nodes must be > 0,"
			print "    where g is the number of nodes of the graph."
			sys.exit(2)
		
		for k in xrange(G.STARTER_NODES):
			startnode=G.whole_network[random.choice(G.whole_network.keys())]
			starters.append(startnode) #starters is a list of nodes 
		
	
	for i in xrange(len(starters)):
		print "starter=", starters[i].nid,"num of active neigh:",starters[i].num_of_active_neigh()
		starters[i].send_fst_packet()
	
	#print "[*] Starting node %s"%(join([i.id for i in starters], ','))



def kill_nodes():
	
	#CHANGE THE for WHEN HIGH LEVEL WILL BE IMPLEMENTED
	num_of_killed_nodes=0
	G.total_packets_forwarded=0
	for i,node in G.whole_network.iteritems():
		node.tracer_forwarded=0
		unif=random.uniform(0,1)
		if unif < node.dp: #the node is dead
			pdb.set_trace()
			node.dead=1
			num_of_killed_nodes+=1
			
			# node is the dead node
			# neigh are the neighbours of the dead node
			# all the neighbours will send the new ETP
			for neigh,link_neigh in node.rnodes.iteritems():
				if G.whole_network[neigh].dead!=1:
					etp_pack=etp_section()
					etp_pack.build_etp(G.whole_network[neigh],etp_pack.CHANGE_DEAD_NODE,node)

	print "killed nodes:",num_of_killed_nodes
#
# Main loop
# 

def main_loop():
	idx=1
	while G.events:
		if idx==G.EVENTS_LIMIT and idx!=0:
			print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
			print "-----------[[[[ ===== --- BREAK --- ===== ]]]]----------------"
			print "-----------[[[[ ===== ---       --- ===== ]]]]----------------"
			G.events_limit_reached=True
			break
		p=heappop(G.events)
		G.curtime=p.time
		p.exec_pkt() # exec_pkt() returns 1 if the pkt it isn't interesting
		del p		# The packet has been dropped
		idx+=1

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


def print_maps():
	
	print 5 * ""
	for i in G.whole_network:
		for j in G.whole_network[i].int_map:
			print "routes of the node: ",i, "for the destination: ", j
			for k in G.metrics:
				print "optimized for the metric: ",k
				G.whole_network[i].int_map[j].metric_array[k].gw[0].print_route()

def print_statistics():
	
	print 5 * ""
	print "NUMBER OF TRACERS FORWARDED PER NODE" 
	for i in G.whole_network:
		#print "node:",G.whole_network[i].nid," forwarded packets:",G.whole_network[i].tracer_forwarded
		print G.whole_network[i].tracer_forwarded
	#for i in G.whole_network:
	#	print G.whole_network[i].dp
	print "total packets forwarded: ",G.total_packets_forwarded
	



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
	else:
		print "Use 'python ntksim.py -g tests/FILE.ntksim'"
		sys.exit(1)
#	elif mgraph > 0:
#		print "[*] Generating a mesh graph of %d*%d=%d node"%(mgraph,mgraph, mgraph*mgraph)
#		g.gen_mesh_graph(mgraph)
#	elif kgraph > 0:
#		print "[*] Generating a complete graph of %d node"%kgraph
#		g.gen_complete_graph(kgraph)
#	elif rgraph > 0:
#		print "[*] Generating a random graph of %d node"%rgraph
#		g.gen_rand_graph(rgraph)

	#print "[*] Saving the graph to "+outgraph
	#g.print_dot(outgraph)


	# The main loop begins
	graph_=graph()
	graph_.print_graph_len()
	
	for i in G.whole_network:
		print "nodo ",G.whole_network[i].nid, "dp:",G.whole_network[i].dp
	
	start_exploration()
	main_loop()
	#print_maps()
	print_statistics()
	
	
	kill_nodes()
	#start_exploration(new_starters)
	#main_loop()
	#print_statistics()



	if G.events_limit_reached:
		print "[!] Warning: The simulation has been aborted, because the"
		print "             events limit (%d) has been reached"%G.EVENTS_LIMIT

#	print "\n---- Statistics ----"
#	g.dump_stats()

	
if __name__ == "__main__":
    main()
