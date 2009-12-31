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

import os
from random import choice


import ntk.core.andna as andna
import ntk.core.andnswrapper as andnswrapper
import ntk.core.counter as counter
import ntk.core.andnsserver as andnsserver
import ntk.core.radar as radar
import ntk.core.route as maproute
import ntk.core.qspn as qspn
import ntk.core.hook as hook
import ntk.core.p2p as p2p
import ntk.core.coord as coord
import ntk.core.krnl_route as kroute
import ntk.lib.crypto as crypto
import ntk.lib.misc as misc
import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime
from ntk.core.status import StatusManager

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro
from ntk.network import NICManager, Route
from ntk.network.inet import ip_to_str, valid_ids
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
        
        self.keypair = crypto.KeyPair(keys_path=settings.KEY_PAIR_PATH)

        if not os.path.exists(settings.CONFIGURATION_DIR):
            os.mkdir(settings.CONFIGURATION_DIR)
                    
        self.simulated = settings.SIMULATED
        self.simnet = simnet
        self.simme = simme
        self.simsock = sockmodgen

        self.ntkd_status = StatusManager()

        self.nic_manager = NICManager(nics=settings.NICS,
                                      exclude_nics=settings.EXCLUDE_NICS)

        # Load the core modules
        rpcbcastclient = rpc.BcastClient(list(self.nic_manager),
                                         net=self.simnet,
                                         me=self.simme,
                                         sockmodgen=self.simsock)
        self.firstnip = self.choose_first_nip()
        self.maproute = maproute.MapRoute(self.ntkd_status, settings.LEVELS, self.gsize, 
                                          self.firstnip)
        self.radar = radar.Radar(self.ntkd_status, self.time_tick, self.nic_manager,
                                 self.maproute, rpcbcastclient, xtimemod)
        self.maproute.set_radar(self.radar)
        self.neighbour = self.radar.neigh

        logging.log(logging.ULTRADEBUG, 'NtkNode: This is maproute as soon '
                    'as started.')
        logging.log(logging.ULTRADEBUG, self.maproute.repr_me())
        self.etp = qspn.Etp(self.ntkd_status, self.time_tick, self.radar, self.maproute)

        self.p2p = p2p.P2PAll(self.ntkd_status, self.radar, self.maproute, self.etp)
        self.coordnode = coord.Coord(self.ntkd_status, self.radar, self.maproute, self.p2p)
        self.counter = counter.Counter(self.ntkd_status, self.keypair, self.radar, self.maproute, self.p2p)
        self.andna = andna.Andna(self.ntkd_status, self.keypair, self.counter, self.radar, self.maproute, self.p2p)
        self.andnswrapper = andnswrapper.AndnsWrapper(self.andna, settings.LOCAL_CACHE_PATH, 
                                 misc.read_resolv(settings.RESOLV_PATH), self.reload_snsd_nodes())
        self.andnsserver = andnsserver.AndnsServer(self.andnswrapper)

        logging.log(logging.ULTRADEBUG, 'NtkNode: This is mapcache of coord '
                    'as soon as started.')
        logging.log(logging.ULTRADEBUG, self.coordnode.mapcache.repr_me())
        logging.log(logging.ULTRADEBUG, 'NtkNode: This is mapp2p of coord as '
                    'soon as started.')
        logging.log(logging.ULTRADEBUG, self.coordnode.mapp2p.repr_me())
        self.hook = hook.Hook(self.ntkd_status, self.radar, self.maproute, self.etp,
                              self.coordnode, self.nic_manager)

        self.hook.events.listen('HOOKED', self.p2p.p2p_hook)
        # A complete reset is needed for each hook
        self.hook.events.listen('HOOKED', self.reset)

        if not self.simulated:
            self.kroute = kroute.KrnlRoute(self.neighbour, self.maproute)

    @microfunc()
    def time_tick(self, function, args=(), **kwargs):
        try:
            tstart = xtime.time()
            logging.log(logging.ULTRADEBUG, 'time_tick: start ' + str(function.__name__) +
                str(args))
            function(*args, **kwargs)
        finally:
            tstop = xtime.time()
            logging.log(logging.ULTRADEBUG, 'time_tick: took ' + str(tstop - tstart) +
                ' msec: exit ' + str(function.__name__) + str(args))

    def reload_snsd_nodes(self, snsd_nodes_path=settings.SNSD_NODES_PATH, snsd_nodes=[]):
        for line in misc.read_nodes(settings.SNSD_NODES_PATH):
            result, data = misc.parse_snsd_node(line)
            if not result:
                raise ImproperlyConfigured("Wrong line in "+str(settings.SNSD_NODES_PATH))
            snsd_nodes.append(data)
        return snsd_nodes
    
    def reset(self, oldip=None, newnip=None):
        logging.debug('resetting node')
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
                rpc.MicroUDPServer(self, ('', 269), nic, self.simnet, 
                                   self.simme, self.simsock)

    def launch_tcp_servers(self):
        rpc.MicroTCPServer(self, ('', 269), None, self.simnet, self.simme, 
                           self.simsock)

    def run(self):
        self.initialize()
        self.andnsserver.run()
        
    @microfunc(True)
    def initialize(self):
        # Enable ip forwarding
        if not self.simulated:
            Route.ip_forward(enable=True)
        # Call hook for bootstrap
        # We can assume that the hook (microfunc) will be complete
        # before the first radar scan.
        logging.debug('start Hook.hook')
        self.neighbour.netid = -1
        self.hook.hook()
        # Now keep doing radar forever.
        logging.debug('start Radar.run')
        self.radar.run()
        # Wait a bit (otherwise problems with coord service)
        xtime.swait(100)
        # Now I'm also participating to service Coord, Counter and Andna
        micro(self.coordnode.participate)
        micro(self.counter.participate)
        micro(self.andna.participate)
        
    def first_activation(self):
        logging.debug('First NIC activation started')
        nip_ip = self.maproute.nip_to_ip(self.firstnip)
        nip_ip_str = ip_to_str(nip_ip)
        logging.debug('First IP choosen (before...) was %s' % nip_ip_str)
        self.nic_manager.activate(ip_to_str(nip_ip))
        logging.debug('First NIC activation done')

    def choose_first_nip(self):
        # Returns a valid NIP.
        nip = [0 for i in xrange(settings.LEVELS)]
        for lvl in reversed(xrange(settings.LEVELS)):
            nip[lvl] = choice(valid_ids(lvl, nip))
        return nip

