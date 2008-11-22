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
from ntk.network import NIC as nic
import ntk.lib.rpc      as rpc
from ntk.lib.micro import micro, allmicro_run
from ntk.wrap.sock import Sock
import ntk.wrap.xtime as xtime
from ntk.config import settings, ImproperlyConfigured

class NtkNode(object):
    def __init__(self, simnet=None, simme=None, sockmodgen=Sock, xtimemod=xtime):

        # Size of a gnode
        self.gsize = 2 ** settings.BITS_PER_LEVEL

        if self.gsize == 1:
            raise ImproperlyConfigured('Gnode size cannot be equal to 1')

        self.simulated = settings.SIMULATED
        self.simnet = simnet
        self.simme = simme
        self.simsock = sockmodgen
        self.load_nics(opt)

        # Load the core modules
        self.inet = inet.Inet(self.ipv, self.bitslvl)

        rpcbcastclient = rpc.BcastClient(self.inet, self.nics.nics.keys(),
                                         net=self.simnet, me=self.simme,
                                         sockmodgen=self.simsock)
        self.radar = radar.Radar(self.inet, rpcbcastclient, xtimemod)
        self.neighbour = self.radar.neigh
        self.maproute = maproute.MapRoute(settings.LEVELS, self.gsize, None)
        self.etp = qspn.Etp(self.radar, self.maproute)

        self.p2p = p2p.P2PAll(self.radar, self.maproute)
        self.coordnode = coord.Coord(self.radar, self.maproute, self.p2p)
        self.hook = hook.Hook(self.radar, self.maproute, self.etp,
                              self.coordnode, self.nics, self.inet)
        self.p2p.listen_hook_ev(self.hook)

        if not self.simulated:
            self.kroute = kroute.KrnlRoute(self.neighbour, self.maproute, self.inet)

    # XXX: TO CHANGE!
    def load_nics(self, opt):
        if type(opt.nics) == str:
                # normalize opt.nics as a list
                opt.nics=[opt.nics]

        if self.simulated:
                self.nics = nic.SimNicAll()
        else:
                self.nics = nic.NicAll( **opt.getdict(['nics', 'exclude_nics']) )
        if self.nics.nics == []:
                raise Exception, "No network interfaces found in the current system"


    def run(self):
        if not self.simulated:
                self.kroute.kroute.route_ip_forward_enable()
                for nic in self.nics.nics:
                        self.kroute.kroute.route_rp_filter_disable(nic)

        rpc.MicroTCPServer(self, ('', 269), None, self.simnet, self.simme, self.simsock)
        rpc.MicroUDPServer(self, ('', 269), None, self.simnet, self.simme, self.simsock)

        self.radar.run()
        self.hook.hook()
