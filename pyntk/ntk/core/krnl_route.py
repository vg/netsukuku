# -*- coding: utf-8 -*-
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
from ntk.core.route import NullRem, DeadRem

class KrnlRoute(object):
    def __init__(self, neigh, maproute):

        self.maproute = maproute
        self.neigh = neigh
        self.multipath = settings.MULTIPATH

        self.maproute.events.listen('ROUTES_UPDATED', self.routes_updated)

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

    # The rules that this class has to keep satisfied are:
    # given that we are x;
    # at any time, for any v ∈ V,
    #     if Bestᵗ (x → v) = xy...v > 0
    #         x maintains this RULE: destination = v → gateway = y
    #     ∀w ∈ x*
    #         if w ∈ Bestᵗ (x → v) AND Bestᵗ (x → w̃ → v) = xz...v > 0
    #             x maintains this RULE: source w, destination = v → gateway = z

    def routes_updated(self, lvl, dst):
        routes_to_v = self.maproute.node_get(lvl, dst)
        best = routes_to_v.best_route()
        if not best or isinstance(best.rem, DeadRem):
            # RULE: destination = v → unknown
            self.rule_dest_unknown(lvl, dst)
        else:
            # RULE: destination = v → gateway = y
            self.rule_dest_gw(lvl, dst, best.gw)
            for w in self.neigh.neigh_list(in_my_network=True):
                w_lvl_id = self.maproute.routeneigh_get(w)
                if best.contains(w_lvl_id):
                    best_not_w = routes_to_v.best_route_without(w_lvl_id)
                    if best_not_w:
                        # RULE: source w, destination = v → gateway = z
                        self.rule_dest_src_gw(lvl, dst, w, best_not_w.gw)
                    else:
                        # RULE: source w, destination = v → host/network UNREACHABLE.
                        self.rule_dest_src_unreachable(lvl, dst, w)
                else:
                    # RULE: source w, destination = v → gateway = default for v
                    self.rule_dest_src_delete(lvl, dst, w)

    @microfunc()
    def schedule(self, function, args=(), **kwargs):
        '''Schedules an update to the routing table'''
        # The updates are done one at a time.
        # The received data let us obtain information for the destination v.
        try:
            logging.log(logging.ULTRADEBUG, 'KrnlRoute.schedule: start ' + str(function))
            function(*args, **kwargs)
        finally:
            logging.log(logging.ULTRADEBUG, 'KrnlRoute.schedule: exit ' + str(function))

    def rule_dest_unknown(self, *args):
        self.schedule(self.serialized_rule_dest_unknown, args)

    def serialized_rule_dest_unknown(self, lvl, dst):
        # Obtain IP for node
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Maintain this rule
        KRoute.default_route(ipstr, str(lvl_to_bits(lvl)), gateway=None, dev=None)

    def rule_dest_gw(self, *args):
        self.schedule(self.serialized_rule_dest_gw, args)

    def serialized_rule_dest_gw(self, lvl, dst, gw):
        # Obtain IP for node and gateway
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        gwip = gw.ip
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Obtain a IP string for this gateway
        gwipstr = ip_to_str(gwip)
        dev = gw.bestdev[0]
        # Maintain this rule
        KRoute.default_route(ipstr, str(lvl_to_bits(lvl)), gateway=gwipstr, dev=dev)

    def rule_dest_src_gw(self, *args):
        self.schedule(self.serialized_rule_dest_src_gw, args)

    def serialized_rule_dest_src_gw(self, lvl, dst, src, gw):
        # Obtain IP for node and gateway
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        gwip = gw.ip
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Obtain a IP string for this gateway
        gwipstr = ip_to_str(gwip)
        dev = gw.bestdev[0]
        # Obtain each MAC string for this src
        for macsrc in src.macs:
            # Maintain this rule
            KRoute.prev_hop_route(ipstr, str(lvl_to_bits(lvl)), macsrc, gateway=gwipstr, dev=dev)

    def rule_dest_src_unreachable(self, *args):
        self.schedule(self.serialized_rule_dest_src_unreachable, args)

    def serialized_rule_dest_src_unreachable(self, lvl, dst, src):
        # Obtain IP for node
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Obtain each MAC string for this src
        for macsrc in src.macs:
            # Maintain this rule
            KRoute.prev_hop_route(ipstr, str(lvl_to_bits(lvl)), macsrc, gateway=None, dev=None)

    def rule_dest_src_delete(self, *args):
        self.schedule(self.serialized_rule_dest_src_delete, args)

    def serialized_rule_dest_src_delete(self, lvl, dst, src):
        # Obtain IP for node
        nip = self.maproute.lvlid_to_nip(lvl, dst)
        ip  = self.maproute.nip_to_ip(nip)
        # Obtain a IP string for the node
        ipstr = ip_to_str(ip)
        # Obtain each MAC string for this src
        for macsrc in src.macs:
            # Maintain this rule
            KRoute.prev_hop_route_default(ipstr, str(lvl_to_bits(lvl)), macsrc)

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

