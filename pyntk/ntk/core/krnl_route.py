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
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block
from ntk.network import Route as KRoute
from ntk.network.inet import ip_to_str, lvl_to_bits

class KrnlRoute(object):
    def __init__(self, neigh, maproute):

        self.maproute = maproute
        self.neigh = neigh
        self.multipath = settings.MULTIPATH

        self.maproute.events.listen('ROUTE_NEW', self.route_new)
        self.route_new_calls = []
        self.maproute.events.listen('ROUTE_DELETED', self.route_deleted)
        self.route_deleted_calls = []
        self.maproute.events.listen('ROUTE_REM_CHGED', self.route_rem_changed)
        self.route_rem_changed_calls = []

        self.neigh.events.listen('NEIGH_NEW', self.neigh_new, priority=5)
        self.neigh_new_calls = []
        self.neigh.events.listen('NEIGH_DELETED', self.neigh_deleted, 
                                 priority=15)
        self.neigh_deleted_calls = []
        self.neigh.events.listen('NEIGH_REM_CHGED', self.neigh_rem_changed, 
                                 priority=5)
        self.neigh_rem_changed_calls = []


    def route_new(self, lvl, dst, gw, rem):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.route_new_calls.append((lvl, dst, gw, rem, 
                        self.maproute.node_get(lvl, dst).nroutes()))
        self._route_new()
        micro_block()

    @microfunc()
    def _route_new(self):
        lvl, dst, gw, rem, nroutes = self.route_new_calls.pop(0)
        # Obtain IP for node and gateway
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        neighgw = self.neigh.id_to_neigh(gw)
        gwip = neighgw.ip
        # Did we already have one route to this node?
        existing = nroutes >= 2
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Obtain a IP string for this gateway
        dev = neighgw.bestdev[0]
        gwipstr = ip_to_str(gwip)
        # Do we have multipath
        if self.multipath:
            # Add new route
            self.neigh.waitfor_gw_added(gw)
            KRoute.add(ipstr, lvl_to_bits(lvl), dev, gwipstr)
        else:
            # Add or eventually change best route
            if existing:
                # Eventually change
                # Obtain a IP string for the best gateway
                node = self.maproute.node_get(lvl, dst)
                best = node.best_route()
                newgw = best.gw.id
                newgw_neigh = best.gw
                newgw_dev = newgw_neigh.bestdev[0]
                newgw_gwipstr = ip_to_str(newgw_neigh.ip)
                # Change route in the kernel
                self.neigh.waitfor_gw_added(newgw)
                KRoute.change(ipstr, lvl_to_bits(lvl), newgw_dev, 
                              gateway=newgw_gwipstr)
            else:
                # Add
                self.neigh.waitfor_gw_added(gw)
                KRoute.add(ipstr, lvl_to_bits(lvl), dev, gwipstr)

    def route_deleted(self, lvl, dst, gwip):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.route_deleted_calls.append((lvl, dst, gwip, 
                        self.maproute.node_get(lvl, dst).is_free()))
        self._route_deleted()
        micro_block()

    @microfunc()
    def _route_deleted(self):
        lvl, dst, gwip, isfree = self.route_deleted_calls.pop(0)
        # Obtain a IP string for the node
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        ipstr = ip_to_str(ip)
        # Obtain a IP string for the old gateway
        gwipstr = ip_to_str(gwip)
        # Do we have multipath?
        if self.multipath:
            # Just remove old route from kernel
            KRoute.delete(ipstr, lvl_to_bits(lvl), gateway=gwipstr)
        else:
            # Is there a new best route for this node?
            if isfree:
                # No more routes, just remove old route from kernel
                KRoute.delete(ipstr, lvl_to_bits(lvl), gateway=gwipstr)
            else:
                # Obtain a IP string for the new gateway
                node = self.maproute.node_get(lvl, dst)
                best = node.best_route()
                newgw = best.gw.id
                newgw_neigh = best.gw
                newgw_dev = newgw_neigh.bestdev[0]
                newgw_gwipstr = ip_to_str(newgw_neigh.ip)
                # Change route in the kernel
                KRoute.change(ipstr, lvl_to_bits(lvl), newgw_dev, 
                              gateway=newgw_gwipstr)

    def route_rem_changed(self, lvl, dst, gw, rem, oldrem):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.route_rem_changed_calls.append((lvl, dst, gw, rem, oldrem))
        self._route_rem_changed()
        micro_block()

    @microfunc()
    def _route_rem_changed(self):
        lvl, dst, gw, rem, oldrem = self.route_rem_changed_calls.pop(0)
        # Do we have multipath?
        if not self.multipath:
            # We might have a different best route now
            # Obtain a IP string for the node
            nip = self.maproute.lvlid_to_nip(lvl, dst)
            ip  = self.maproute.nip_to_ip(nip)
            ipstr = ip_to_str(ip)
            # Obtain a IP string for the actual best gateway
            node = self.maproute.node_get(lvl, dst)
            best = node.best_route()
            newgw = best.gw.id
            newgw_neigh = best.gw
            newgw_dev = newgw_neigh.bestdev[0]
            newgw_gwipstr = ip_to_str(newgw_neigh.ip)
            # Change route in the kernel
            self.neigh.waitfor_gw_added(newgw)
            KRoute.change(ipstr, lvl_to_bits(lvl), newgw_dev, 
                          gateway=newgw_gwipstr)

    def neigh_new(self, neigh):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.neigh_new_calls.append(neigh)
        self._neigh_new()
        micro_block()

    @microfunc()
    def _neigh_new(self):
        neigh = self.neigh_new_calls.pop(0)
        ipstr = ip_to_str(neigh.ip)
        dev = neigh.bestdev[0]
        KRoute.add_neigh(ipstr, dev)
        logging.debug('ANNOUNCE: gw ' + str(neigh.id) + ' added.')
        self.neigh.announce_gw_added(neigh.id)

    def neigh_rem_changed(self, neigh, oldrem, before_changed_link):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.neigh_rem_changed_calls.append((neigh, oldrem))
        self._neigh_rem_changed()
        micro_block()

    @microfunc()
    def _neigh_rem_changed(self):
        neigh, oldrem = self.neigh_rem_changed_calls.pop(0)
        ipstr = ip_to_str(neigh.ip)
        dev = neigh.bestdev[0]
        KRoute.change_neigh(ipstr, dev)

    def neigh_deleted(self, neigh, before_dead_link):
        # We'll do the real thing in a microfunc, but make sure
        # to have a chance to get scheduled as soon as possible
        # and obtain immediately any data that is susceptible to change.
        self.neigh_deleted_calls.append(neigh)
        self._neigh_deleted()
        micro_block()

    @microfunc()
    def _neigh_deleted(self):
        neigh = self.neigh_deleted_calls.pop(0)
        ipstr = ip_to_str(neigh.ip)
        self.neigh.waitfor_gw_removable(neigh.id)
        KRoute.delete_neigh(ipstr)

