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

import ntk.core.radar   as radar
import ntk.core.route   as maproute
import ntk.core.qspn    as qspn
import ntk.core.hook    as hook
import ntk.core.p2p     as p2p
import ntk.core.coord   as coord
import ntk.core.krnl_route as kroute
import ntk.network.inet as inet
import ntk.network.nic  as nic
import ntk.lib.rpc      as rpc
from ntk.lib.micro import micro, allmicro_run

class NtkNode:
    def __init__(self, opt, IP=None):

        self.opt = opt

	self._set_ipv( **opt.getdict(['levels', 'ipv']) )

	self.nics = nic.NicAll( **opt.getdict(['nics', 'exclude_nics']) )
	if self.nics.nics == []:
		raise Exception, "No network interfaces found in the current system"

	# Load the core modules
	self.inet	= inet.Inet(self.ipv, self.bitslvl)

        self.radar      = radar.Radar(self.inet, **opt.getdict(
				      ['bquet_num', 'max_neigh', 'max_wait_time']) )
	self.neighbour  = self.radar.neigh
	self.maproute   = maproute.MapRoute(self.levels, self.gsize, me=IP)
	self.etp        = qspn.Etp(self.radar, self.maproute)

   	self.p2p	= p2p.P2PAll(self.radar, self.maproute)
   	self.coordnode	= coord.Coord(self.radar, self.maproute, self.p2p)
	self.hook       = hook.Hook(self.radar, self.maproute, self.etp,
				    self.coordnode, self.nics, self.inet)
	self.p2p.listen_hook_ev(self.hook)

	self.kroute     = kroute.KrnlRoute(self.neighbour, self.maproute, self.inet, 
						**opt.getdict(['multipath']))


    def _set_ipv(self, levels = 4, ipv = inet.ipv4):
    	self.levels = levels
	self.ipv    = ipv

	self.bitslvl= inet.ipbit[ipv] / levels	# how many bits of the IP
						# addres are allocate to each gnode
	self.gsize  = 2**(self.bitslvl)		# size of a gnode

	if self.gsize == 1:
		raise OptErr, "the gnode size cannot be equal to 1"

    def run(self):

	self.kroute.kroute.route_ip_forward_enable()
	for nic in self.nics.nics:
		self.kroute.kroute.route_rp_filter_disable(nic)
	

	tcp_server = rpc.TCPServer(self)
	micro(tcp_server.serve_forever)

	udp_server = rpc.UDPServer(self)
	micro(udp_server.serve_forever)

        self.radar.run()
	self.hook.hook()
