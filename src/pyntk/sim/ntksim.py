#!/usr/bin/env python
#
#  This file is part of Netsukuku
#  (c) Copyright 2007 Andrea Milazzo aka Mancausoft <andreamilazzo@mancausoft.org>
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
