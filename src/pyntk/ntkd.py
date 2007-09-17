#!/usr/bin/env python
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

import sys

from lib.opt import Opt, OptErr
import core.radar   as radar
import core.route   as maproute
import core.qspn    as qspn
import core.hook    as hook
import core.p2p     as p2p
import core.coord   as coord
import core.krnl_route as kroute
import network.inet as inet
from config import *

class Ntkd:
    def __init__(self, opt, IP=None):

	self._set_ipv( opt.getdict(['levels', 'ipv']) )

	self.nics = NicAll( opt.getdict(['nics', 'exclude_nics']) )
	if self.nics.nics == []:
		raise Exception, "No network interfaces found in the current system"

	# Load the core modules
	self.inet	= inet.Inet(self.ipv, self.bitslvl)
        self.radar      = radar.Radar( opt.getdict(['bquet_num', 'max_neigh', 'max_wait_time']) )
	self.neighbour  = self.radar.neigh

	self.maproute   = maproute.Maproute(self.levels, self.gsize)
	self.etp        = qspn.Etp(self.radar, self.maproute)
	self.hook       = hook.Hook(self.radar, self.maproute, self.etp,
				    self.coordnode, self.nics, self.inet)
	self.kroute     = kroute.KrnlRoute(self.neighbour, self.maproute, self.inet, **opt.getdict(['multipath']))

   	self.p2p	= p2p.P2PAll(self.radar, self.maproute, self.hook)
   	self.coordnode	= coord.Coord(self.radar, self.maproute, self.p2p)

    def _set_ipv(self, levels = 4, ipv = inet.ipv4):
    	self.levels = levels
	self.ipv    = ipv

	self.bitslvl= inet.ipbit[ipv]/levels	# how many bits of the IP
						# addres are allocate to each gnode
	self.gsize  = 2**(self.bitslvl)		# size of a gnode

	if self.gsize == 1:
		raise OptErr, "the gnode size cannot be equal to 1"

    def run(self):

	self.kroute.route_ip_forward_enable()
	for nic in self.nics.nics:
		self.kroute.route_rp_filter_disable(nic)
        self.radar.run()
	#self.rpc.serve_forever()


class NtkdBroadcast(Ntkd):
    def __init__(self, level, callbackfunc):
	    #TODO 
	    pass

usage = """
ntkd [n=nics_list] [c=config] 


     n=['nic1', 'nic2', ...]		explicit nics to use

     c="/path/to/config/file.conf"	configuration file path

     ipv=4 or ipv=6			IP version
    
     dbg=0..9				debug level (default 0)
     v or version			version
     h or help				this help
"""

def main():

    # load options
    opt = Opt( {'n':'nics', 
	    
	    	'c':'config_file',

		'v':'version',
		'-v':'version',
		'h':'help',
		'-h':'help'
	       } )
    opt.config_file = CONF_DIR + '/netsukuku.conf'
    opt.load_argv(sys.argv)

    if opt.help:
	    print usage
	    sys.exit(1)
    if opt.version:
	    print "NetsukukuD " + VERSION
	    sys.exit(1)

    if opt.config_file:
	    opt.load_file(opt.config_file)
            opt.load_argv(sys.argv)

if __name__ == "__main__":
	main()
