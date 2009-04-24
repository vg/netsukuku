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
# 
# krnl_route.py
#
# Listens to MapRoute generated events, and updates the kernel table
#

from ntk.config import settings
from ntk.lib.event import Event, apply_wakeup_on_event
from ntk.lib.micro import microfunc
from ntk.network import Route as KRoute
from ntk.network.inet import ip_to_str, lvl_to_bits

class KrnlRoute(object):
    def __init__(self, neigh, maproute):
        self.maproute = maproute
        self.neigh = neigh
        self.multipath = settings.MULTIPATH

        self.events =  Event(['KRNL_NEIGH_NEW'])
        
        self.maproute.events.listen('ROUTE_NEW', self.route_new)
        self.maproute.events.listen('ROUTE_DELETED', self.route_deleted)
        self.maproute.events.listen('ROUTE_REM_CHGED', self.route_rem_changed)

        self.neigh.events.listen('NEIGH_NEW', self.neigh_new)
        self.neigh.events.listen('NEIGH_DELETED', self.neigh_deleted)
        self.neigh.events.listen('NEIGH_REM_CHGED', self.neigh_rem_changed)

        self.route_new = apply_wakeup_on_event(self.route_new, 
                                               events=[(self.neigh.events, 'NEIGH_NEW'),
                                                       (self.events, 'KRNL_NEIGH_NEW')])

    @microfunc(True)
    def route_new(self, lvl, dst, gw, rem, event_wait=None):
        
        if not self.multipath and self.maproute.node_get(lvl, dst).nroutes_synced() >= 1:
                # We don't have multipath and we've already set one route.
                return

        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        ipstr = ip_to_str(ip)
        neigh = self.neigh.id_to_neigh(gw)
        dev = neigh.bestdev[0]
        gwipstr = ip_to_str(neigh.ip)
        neigh_node = self.maproute.node_get(*self.maproute.routeneigh_get(neigh))

        if neigh_node.nroutes() > 1:
                # Let's wait to add the neighbour first
                while 1:
                        ev_neigh = event_wait[(self.neigh.events, 'NEIGH_NEW')]()
                        if neigh == ev_neigh:
                                # found
                                break

        if neigh_node.routes_tobe_synced > 0:
                # The routes to neigh are still to be synced, let's wait
                while 1:
                        ev_neigh = event_wait[(self.events, 'KRNL_NEIGH_NEW')]()
                        if neigh == ev_neigh:
                                # found
                                break

        # We can add the route in the kernel
        KRoute.add(ipstr, lvl_to_bits(lvl), dev, gwipstr)

        self.maproute.node_get(lvl, id).routes_tobe_synced-=1


    @microfunc(True)
    def route_deleted(self, lvl, dst, gw):
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        ipstr = ip_to_str(ip)
        neigh = self.neigh.id_to_neigh(gw)
        dev = neigh.bestdev[0]
        gwipstr = ip_to_str(neigh.ip)

        KRoute.delete(ipstr, lvl_to_bits(lvl), gateway=gwipstr)

    def route_rem_changed(self, lvl, dst, gw, rem, oldrem):
        pass

    @microfunc(True)
    def neigh_new(self, neigh):
        ipstr = ip_to_str(neigh.ip)
        dev = neigh.bestdev[0]
        gwipstr = ipstr

        KRoute.add(ipstr, lvl_to_bits(0), dev, gwipstr)

        # TODO: generate the KRNL_NEIGH_NEW event

    def neigh_rem_changed(self, neigh, oldrem=None):
        pass

    @microfunc(True)
    def neigh_deleted(self, neigh):
        ipstr = ip_to_str(neigh.ip)

        KRoute.delete(ipstr, lvl_to_bits(0))
