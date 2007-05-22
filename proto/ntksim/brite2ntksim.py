#!/usr/bin/python
#
# brite2ntksim -- an ad-hoc topology convertion tool (BRITE -> Netsukuku Simulator)
#   by Alessandro Ordine
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

import random
import math
import string
import sys
from random import Random

LAMBDA=10.42		#to be computed, 10.42 allows to have the 20% of the nodes with a 
			#death probability between 7% and 10%


print "brite2ntksim -- an ad-hoc topology convertion tool (BRITE -> Netsukuku Simulator)"
print " by Alessandro Ordine"
print
if len(sys.argv) < 3:
	print "Usage: bryte2ns <in_brite_file> <out_ns_file>"
	print
	sys.exit(0)

print "Converting "+sys.argv[1] +" --> "+sys.argv[2]+".     Reading input (BRITE) file... ",
sys.stdout.flush()

f = open(sys.argv[1])

state = 0 #0-> skipping; 1->reading nodes; 2->reading edges;

#keep the bandwidth from the BRITE file (or use the "linkBW" parameter from the tcl script) 
keepBriteBandwith = 0

nodes = []
edges = []

for line in f.readlines() :
	list = string.split(line)
	if len(list)==0 or list[0][0] == '#':
		continue

	if state == 0 :
		if list[0] == "Nodes:" :
			state = 1

	elif state == 1 :
		if list[0] == "Edges:" :
			state = 2
		else :
			#just keep node index
			nodes.append(list[0])

	elif state == 2 :
		#just keep from, to, delay and bw
		edges.append([list[1], list[2], list[3], list[4], list[5]])

f.close()

print " done!    Creating output (NS) file... ",
sys.stdout.flush()

#brite file parsing done. It's time to create the ns file

o = open(sys.argv[2], "w+")

dp_processed_nodes={}

for edge in edges :
	
	#assign death probability

	if not dp_processed_nodes.has_key(str(nodes.index(edge[0]))):
		unif1=random.uniform(0,1)
		dp1=-1/LAMBDA*math.log(1-unif1*(1-math.exp(-LAMBDA*0.1)))
		dp_processed_nodes[str(nodes.index(edge[0]))]=dp1
		
	if not dp_processed_nodes.has_key(str(nodes.index(edge[1]))):
		unif2=random.uniform(0,1)
		dp2=-1/LAMBDA*math.log(1-unif2*(1-math.exp(-LAMBDA*0.1)))
		dp_processed_nodes[str(nodes.index(edge[1]))]=dp2
	
	o.write(str(nodes.index(edge[0]))+" -- "+str(nodes.index(edge[1]))+" -- "+str(round(float(edge[3]),2))+" -- "+str(round(float(edge[4])*1000,2))+" -- "+str(round(float(edge[4])*1000,2))+" -- "+ str(round(dp_processed_nodes[str(nodes.index(edge[0]))],3))+" -- "+str(round(dp_processed_nodes[str(nodes.index(edge[1]))],3))+"\n")

o.write("\nNumber of nodes:"+str(len(nodes)))
o.close()
print "done!"
