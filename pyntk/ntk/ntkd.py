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
from ntk.network.inet import ip_to_str
from ntk.wrap.sock import Sock
from random import choice

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
        self.firstnip = self.choose_first_nip()
        self.maproute = maproute.MapRoute(settings.LEVELS, self.gsize, self.firstnip)
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

        if not self.simulated:
            self.kroute = kroute.KrnlRoute(self.neighbour, self.maproute)

    def reset(self, oldnip=None, newnip=None):
        logging.debug('resetting node')
        # clean our map route
        self.maproute.map_reset()
        # close the server socket
        rpc.stop_tcp_servers()
        logging.debug('TCP servers stopped')
        rpc.stop_udp_servers()
        logging.debug('UDP servers stopped')
        # restart servers
        self.launch_udp_servers()
        logging.debug('UDP servers launched')
        self.launch_tcp_servers()
        logging.debug('TCP servers launched')

    def launch_udp_servers(self):
        if not self.simulated:
            for nic in self.nic_manager:
                self.nic_manager[nic].filtering(enable=False)
                rpc.MicroUDPServer(self, ('', 269), nic, self.simnet, self.simme, self.simsock)

    def launch_tcp_servers(self):
        rpc.MicroTCPServer(self, ('', 269), None, self.simnet, self.simme, self.simsock)

    def run(self):
        self.initialize()

    @microfunc(True)
    def initialize(self, event_wait=None):
        # enable ip forwarding
        if not self.simulated:
            Route.ip_forward(enable=True)
        # first, activate the interfaces
        self.first_activation()
        # start just UDP servers
        logging.debug('start UDP servers')
        self.launch_udp_servers()
        # then, do just one radar scan
        logging.debug('start Radar.radar')
        micro(self.radar.radar)
        logging.debug('waiting SCAN_DONE')
        msg = event_wait[(self.radar.events, 'SCAN_DONE')]() # waits for the end of radar
        logging.debug('got SCAN_DONE')
        # re-initialize the UDP servers sockets
        rpc.stop_udp_servers()
        logging.debug('UDP servers stopped')
        self.launch_udp_servers()
        logging.debug('UDP servers launched')
        # clean our map route
        self.maproute.map_reset()
        logging.debug('MapRoute cleaned')
        # now the real hooking can be done
        logging.debug('start Hook.hook')
        self.hook.hook()
        logging.debug('waiting HOOKED')
        msg = event_wait[(self.hook.events, 'HOOKED')]() # waits for the end of hook
        logging.debug('got HOOKED')
        # re-initialize the UDP servers sockets, and start the TCP servers
        rpc.stop_udp_servers()
        logging.debug('UDP servers stopped')
        self.launch_udp_servers()
        logging.debug('UDP servers launched')
        self.launch_tcp_servers()
        logging.debug('TCP servers launched')
        # re-initialize the UDP client socket before enabling radar
        self.radar.radar_reset()
        logging.debug('radar reset (client broadcast reset)')
        # From now on a complete reset is needed for each new hook
        self.hook.events.listen('HOOKED', self.reset)
        # Now I'm also participating to service Coord
        self.coordnode.participate()
        # now keep doing radar forever.
        logging.debug('start Radar.run')
        self.radar.run()

    def first_activation(self):
        logging.debug('First NIC activation started')
        nip_ip = self.maproute.nip_to_ip(self.firstnip)
        nip_ip_str = ip_to_str(nip_ip)
        logging.debug('First IP choosen %s' % nip_ip_str)
        self.nic_manager.activate(ip_to_str(nip_ip))
        logging.debug('First NIC activation done')

    def choose_first_nip(self):
        # TODO a valid IP for our IP version, possibly one that is not choosen by normal node.
        return [choice(xrange(self.gsize)), choice(xrange(self.gsize)), 168, 192]

