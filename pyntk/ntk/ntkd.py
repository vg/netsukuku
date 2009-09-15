#!/usr/bin/env python
##
# This file is part of Netsukuku
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

from ntk.lib.log import logger as logging
import ntk.core.radar as radar
import ntk.core.route as maproute
import ntk.core.qspn as qspn
import ntk.core.hook as hook
import ntk.core.p2p as p2p
import ntk.core.coord as coord
import ntk.core.krnl_route as kroute
import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime
from ntk.lib.event import Event, apply_wakeup_on_event
from ntk.lib.micro import microfunc

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.micro import micro, allmicro_run
from ntk.network import NICManager, Route
from ntk.wrap.sock import Sock

class NtkNode(object):

    def __init__(self,
                 simnet=None,
                 simme=None,
                 sockmodgen=Sock,
                 xtimemod=xtime,
                 simsettings=None):

        global settings

        if simsettings is not None:
            settings = simsettings

        # Size of a gnode
        self.gsize = 2 ** settings.BITS_PER_LEVEL

        if self.gsize == 1:
            raise ImproperlyConfigured('Gnode size cannot be equal to 1')

        self.simulated = settings.SIMULATED
        self.simnet = simnet
        self.simme = simme
        self.simsock = sockmodgen

        self.nic_manager = NICManager(nics=settings.NICS,
                                      exclude_nics=settings.EXCLUDE_NICS)

        # Load the core modules
        rpcbcastclient = rpc.BcastClient(list(self.nic_manager),
                                         net=self.simnet,
                                         me=self.simme,
                                         sockmodgen=self.simsock)
        self.radar = radar.Radar(rpcbcastclient, xtimemod)
        self.neighbour = self.radar.neigh
        self.maproute = maproute.MapRoute(settings.LEVELS, self.gsize, None)
        self.etp = qspn.Etp(self.radar, self.maproute)

        self.p2p = p2p.P2PAll(self.radar, self.maproute)
        self.coordnode = coord.Coord(self.radar, self.maproute, self.p2p)
        self.hook = hook.Hook(self.radar, self.maproute, self.etp,
                              self.coordnode, self.nic_manager)

        # The method initialize must be able to wait for an event of Radar and Hook
        self.initialize = apply_wakeup_on_event(self.initialize, 
                                              events=[(self.radar.events, 'SCAN_DONE'),
                                                      (self.hook.events, 'HOOKED')])

        self.p2p.listen_hook_ev(self.hook)
        self.hook.events.listen('HOOKED', self.reset)

        if not self.simulated:
            self.kroute = kroute.KrnlRoute(self.neighbour, self.maproute)

    def reset(self, oldnip=None, newnip=None):
        # stop the radar is really needed?
        self.radar.stop()
        # close the server socket
        rpc.stop_servers()
        # restart servers
        self.launch_servers()
        # restart radar
        self.radar.run()

    def launch_servers(self):
        if not self.simulated:
            for nic in self.nic_manager:
                self.nic_manager[nic].filtering(enable=False)
                rpc.MicroUDPServer(self, ('', 269), nic, self.simnet, self.simme, self.simsock)

        rpc.MicroTCPServer(self, ('', 269), None, self.simnet, self.simme, self.simsock)

    def run(self):
        if not self.simulated:
            Route.ip_forward(enable=True)

        self.launch_servers()

        self.initialize()

    @microfunc(True)
    def initialize(self, event_wait = None):
        # first hook to activate interfaces
        logging.debug('start Hook.hook')
        self.hook.hook()
        logging.debug('waiting HOOKED')
        msg = event_wait[(self.hook.events, 'HOOKED')]() # waits for the end of first hook
        logging.debug('got HOOKED')
        # but we don't care about the netid that has been choosen
        self.radar.netid = -1
        # and do_reply has to be false yet. It will be activated with next hook.
        self.radar.do_reply = False
        # just one radar 
        logging.debug('start Radar.radar')
        micro(self.radar.radar)
        logging.debug('waiting SCAN_DONE')
        msg = event_wait[(self.radar.events, 'SCAN_DONE')]() # waits for the end of radar
        logging.debug('got SCAN_DONE')
        # now the real hooking can be done
        logging.debug('start Hook.hook')
        self.hook.hook()
        logging.debug('waiting HOOKED')
        msg = event_wait[(self.hook.events, 'HOOKED')]() # waits for the end of hook
        logging.debug('got HOOKED')
        # now keep doing radar forever.
        logging.debug('start Radar.run')
        self.radar.run()

