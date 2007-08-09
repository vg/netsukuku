#!/usr/bin/env python
#
#  This file is part of Netsukuku
#  (c) Copyright 2007 
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

import getopt
import sys
import random
from string import join
sys.path.append('..')
from core import ntkd



def send(sck, data, flag=0):
    pck = packet (sck.id, sck.addr, sck.r_addr, sck.chan, data, sck.t)
    

def connect(self, sck):
    n = graph.ip2node(sck.addr)
    if not n.is_neigh(graph.ip2node(sck.r_addr)) \
       or graph.ip2node(sck.addr).ntkd == None:
        return false
    graph.ip2node(sck.r_addr).ntkd.accept(sck.chan) #TODO: check
    return true


class packet:
    total_pkts=0

    def __str__(self):
        return self.tracer_to_str()

    def __cmp__(self, other):
        return self.time-other.time

    def __init__(self, sck, src, dst, chan, data,t=None):
        self.sck = sck
        self.src = src
        self.dst = dst
        self.chan = chan
        self.data = data
          
        if t:
            delay = t
        elif dst.id != src.id:
            delay=self.src.rtt[dst.id] #TODO: check
        else:
            delay=0

        self.time = G.curtime+delay
        heappush(G.events, self)

    def exec_pkt():
       self.chan.send(self.data)
       total_pkts += 1


class G:
    #
    # defines
    #
    default_rtt = 100          # in millisec
    delta_rtt   = default_rtt/10

    max_routes  = 1

    starter_nodes   = 1
    events_limit    = 0


    #
    # globals
    #
    curtime=0
    events=[]

    #
    # flags
    #
    change_graph=false
    verbose=false
    events_limit_reached=false

class node:
    total_nodes=0

    def __hash__(self):
        return hash(self.id) 

    def __str__(self):
        return self.me.id

    def __init__(self, id, rnodes):
        self.rtt={}
        self.id=id

        for (id, rtt) in rnodes:
            self.rtt[node]=rtt
            self.rnode_id.append(node)
    
    def is_neigh(self, b):
        return (b.id in self.rnode)



 
class graph:
    graph={}
    graph_len=0

    def load_graph(self, file):
        """the format of the file must be:
            
            nodex -- nodey -- weightxy -- weightyx
            nodex -- nodez -- weightxz -- weightzx
            ...
           the "-- weightxy -- weightyx" part is optional.
           all the lines of the file which aren't in this form are
           ignored. in other words, you can use graphviz (.dot) file.
        """
        nodes={}
        
        f=open(file)
        for l in f:
            w=re.split(' *-- *', l.strip())
            if len(w) == 1:
                continue

            if len(w) < 3:
                weightxy=g.default_rtt
            else:
                weightxy=w[2]
            if len(w) < 4:
                weightyx=weightxy
            else:
                weightyx=w[3]

            if w[0] not in nodes:
                nodes[w[0]]=[]
            if w[1] not in nodes:
                nodes[w[1]]=[]
            nodes[w[0]].append((w[1], weightxy))
            nodes[w[1]].append((w[0], weightyx))

        for id,rnodes in nodes.iteritems():
            graph.graph[id]=node(id, rnodes)
        graph.graph_len=len(graph.graph)

    def ip2node(self, addr):
        for n in nodes:
            if n.ntkd.ip == addr:
                return n
        return none

    def gen_complete_graph(self, k):
        k+=1
        for i in xrange(1,k):
            nstr='n%d'%i
            rnodes=[('n%d'%o, g.default_rtt) for o in xrange(1, k) if o != i]
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
                    rnodes.append(('n%d'%((x-1)*k+y), g.default_rtt))
                if x < k-1:
                    #right
                    rnodes.append(('n%d'%((x+1)*k+y), g.default_rtt))
                if y > 0:
                    #down
                    rnodes.append(('n%d'%(x*k+(y-1)), g.default_rtt))
                if y < k-1:
                    #up
                    rnodes.append(('n%d'%(x*k+(y+1)), g.default_rtt))

                n=node(nstr, rnodes)
                graph.graph[nstr]=n

        graph.graph_len=len(graph.graph)

    def gen_rand_graph(self, k):
        nodes={}
        rtt={}
        if k < 2:
            print "the number of nodes for the random graph must be >= 2"
            sys.exit(2)

        for i in xrange(k):
            if i not in nodes:
                nodes[i]=[]
                rtt[i]=[]
            r=random.randint(1, max(k/4, 1))
            for xx in xrange(r):
                ms=random.randint(g.delta_rtt, g.default_rtt*2)

                # choose a random rnode which is not  i  and
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
        f.write("graph g {\n")
        f.write("\tnode [shape=circle]\n\n")
        for id, node in graph.graph.iteritems():
            for l in node.rnode_id:
                f.write("\t"+id+" -- "+l+"   [weight=%d]"%node.rtt[l]+"\n")
        f.write("}\n")
        f.close() 


    def reset_stats(self):
        for id, node in graph.graph.iteritems():
            node.tracer_forwarded=0

        packet.total_pkts=packet.total_tpkts=0
        G.curtime=0

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
        i.id = Ntkd("v") 
    print "[*] Starting node %s"%(join([i.id for i in starters], ','))

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

    print g
    print dir(g.graph)
    # The main loop begins
    start_exploration()
    main_loop()


#   if G.change_graph:
#       print "\n---- Statistics ----"
#       g.dump_stats()
#       print "[*] Resetting the statistics"
#       g.reset_stats()
#       print "[*] Modifying %d links of the graph"%G.change_graph
#       change_graph(G.change_graph)
#       print "[*] Starting the new exploration"
#       main_loop()
#
#
#   if G.events_limit_reached:
#       print "[!] Warning: The simulation has been aborted, because the"
#       print "             events limit (%d) has been reached"%G.EVENTS_LIMIT
#
#   print "\n---- Statistics ----"
#   g.dump_stats()

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
		if p.exec_pkt():
			# The packet has been dropped
			del p
		idx+=1



if __name__ == "__main__":
    main()
