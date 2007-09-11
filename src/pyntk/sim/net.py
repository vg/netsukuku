##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
#
# This source code is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This source code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# Please refer to the GNU Public License for more details.
#
# You should have received a copy of the GNU Public License along with
# this source code; if not, write to:
# Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

from heapq   import heappush, heappop
from random  import randint, choice
from sim     import Event
from G       import G
from opt     import Opt

class ENet(Exception):
    errstr="""Generic network error"""
    pass

class ENotNeigh(ENet):
    errstr="""Specified destination is not a neighbour"""
    pass

class ESendMsg(ENet):
    errstr="""`msg' is not a string"""
    pass

class Link(object):
    __slots__ = [ 'rtt', 'bw' ]
    def __init__(self, rtt, bw, rand=0):
        self.rtt=rtt #in millisec
	self.bw =bw  #byte per millisec
	if rand:
		self.set_random()
        self.set_average()
    
    def set_random(self, rtt_range=(4,32), bw_range=(8, 128)):
        self.rtt=randint(*rtt_range)
        self.bw =randint(*bw_range)
        self.set_average()

    def set_average(self):
        self.average = float(self.rtt+self.bw)/2

    
class Node:
    def __init__(self, ip=None, neighs={}):
        self.ip=ip
	if self.ip == None:
		self.ip=randint(1, 2**32-1)

	# {Node: Link}
	self.neighbours = {}

	self.recv_queue = []

    def __hash__(self):
        return self.ip

    def neigh_add(self, n, link):
        self.neighbours[n]=link
	n.neighbours[self]=link # the links are symmetric
    
    def neigh_del(self, n):
        if n in self.neighbours:
		del self.neighbours[n]
		del n.neighbours[self]

    def neigh_del_all(self):
        for n in self.neighbours:
		self.neigh_del(n)

    def calc_time(self, d, sz):
        """Return the time necessary to send a packet of 
	   size `sz', from `self' to `d'"""
        return self.neighbours[d].rtt+float(sz)/self.neighbours[d].bw

    def send(self, dst, msg):
        """dst: the destination neighbour
	   msg: the packet itself. It must be a string!
	   
	   On error hell will be raised"""
	if not isinstance(msg, str):
		raise ESendMsg, ESendMsg.errstr

	if dst not in self.neighbours:
		raise ENotNeigh, ENotNeigh.errstr
	
	ev = Event(self.calc_time(dst, len(msg)), dst.recv, (self, msg))
	G.sim.ev_add(ev)

    def recv(self, sender, msg):
        """Adds to the received queue the msg"""
	self.recv_queue.append((sender, msg))

class Net:
    def __init__(self):
	self.net={} # {IP: Node} The dict of nodes

    def node_add(self, ip=None, neighs={}):
	n=Node(ip, neighs)
        self.net[n.ip]=n
	return n

    def node_del(self, n):
        n.neigh_del_all() 
        del self.net[n.ip]

    def node_get(self, ip):
        if ip not in self.net:
		return self.node_add(ip)
	else:
		return self.net[ip]
    
    def complete_net_build(self, k):
    	for i in xrange(k):
		node = self.node_get(i)
		for o in xrange(k):
			if o == i:
				continue
			l=Link()
			l.set_random()
			node.neigh_add(self.node_get(o), l)

    def mesh_net_build(self, k):
    	for x in xrange(k):
    		for y in xrange(k):
			node = self.node_get(x*k+y)

    			if x > 0:
    				#left
				l=Link()
				l.set_random()
				node.neigh_add(self.node_get((x-1)*k+y), l)
    			if x < k-1:
    				#right
				l=Link()
				l.set_random()
				node.neigh_add(self.node_get((x+1)*k+y), l)
    			if y > 0:
    				#down
				l=Link()
				l.set_random()
				node.neigh_add(self.node_get(x*k+(y-1)), l)
    			if y < k-1:
    				#up
				l=Link()
				l.set_random()
				node.neigh_add(self.node_get(x*k+(y+1)), l)

    def rand_net_build(self, k):
    	for i in xrange(k):
		node = self.node_get(i)
		
		# how many neighbour
    		nb   = randint(1, max(k/8, 1))

		rn = range(k)
    		for j in xrange(nb):
			l=Link()
			l.set_random()

    			# Choose a random rnode which is not i and
    			# which hasn't been already choosen
			def not_me_or_already_neigh(x): 
				return x!=i and self.node_get(x) not in node.neighbours
			rn=filter(not_me_or_already_neigh,  rn)
			if rn == []:
				break
			
			neigh = self.node_get(choice(rn))
			node.neigh_add(neigh, l)

    def net_file_load(self, file):
        """A sample file:

        --- BEGIN ---
        net = {

        	'A' : [ ('B', 'rtt=10, bw=6'), ('C', 'rand=1') ],

        	'B' : [ ('node D', 'rtt=2, bw=9') ],
            
    	'C' : [],

    	'node D' : []
        }
        ---  END  ---
        """
        o = Opt()
        o.opt_load_file(file)

        count = 0
        idtoip = {}
        for x in o.net:
    	    idtoip[x]=count
    	    count+=1

            for x in o.net:
		    node = self.node_get(idtoip[x])

    	    for y, link in o.net[x]:
    		    ynode = self.node_get(idtoip[y])
    		    exec 'node.neigh_add(ynode,'+link+')'

    def net_dot_dump(self, fd):
        """Dumps the net to a .dot graphviz file.

	fd: an open file descriptor"""
	fd.write("graph g {\n\tnode [shape=circle]\n\n")
	
	t = {}
        for ip in self.net:
		n = self.net[ip]
		if n not in t: t[n]=[]

		for neigh in n.neighbours:
			if neigh not in t:
				t[neigh]=[]
			if neigh in t[n] or n in t[neigh]:
				continue
			t[n].append(neigh)
			t[neigh].append(n)

			l=n.neighbours[neigh]
			fd.write("\t"+str(n.ip)+" -- "+str(neigh.ip)+" [weight=%d]"%l.average+"\n")
	fd.write("}\n")
