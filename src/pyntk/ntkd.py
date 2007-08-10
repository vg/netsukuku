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

from lib.opt import Opt, OptErr
import core.radar   as radar
import core.route   as maproute
import core.qspn    as qspn
import network.inet as inet
from lib.misc import dict_remove_none as drn

class Ntkd:
    def __init__(self, opt, IP=None):

	self._set_ipv(**drn({'levels' : opt.levels, 'ipv' : opt.ipv}))

        self.radar      = radar.Radar( **drn({'multipath' : opt.multipath, 
                                              'bquet_num' : opt.bquet_num,
                                              'max_neigh' : opt.max_neigh,
                                              'max_wait_time' : opt.max_wait_time)})
				     )
	self.neighbour  = radar.Neighbour(**drn({'multipath' : opt.multipath,
		                                 'max_neigh' : opt.max_neigh}))

	self.maproute   = maproute.Maproute(self.levels, self.gsize)
	self.etp        = qspn.Etp(self.neighbour, self.maproute)


    def _set_ipv(self, levels = 4, ipv = net.ipv4):
    	self.levels = levels
	self.ipv    = ipv
	self.gsize  = 2**(inet.ip_bit(ipv)/levels)	# size of a gnode
	if self.gsize == 1:
		raise OptErr, "the gnode size cannot be equal to 1"

    def run(self):

        self.radar.run()
	#self.rpc.serve_forever()


class NtkdBroadcast(Ntkd):
    def __init__(self, level, callbackfunc):
	    #TODO
	    pass
