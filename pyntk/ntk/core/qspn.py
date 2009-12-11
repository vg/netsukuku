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

import time

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime

from ntk.core.route import NullRem, DeadRem
from ntk.lib.event import Event, apply_wakeup_on_event
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block, DispatcherToken
from ntk.lib.rpc import RPCError
from ntk.network.inet import ip_to_str
from ntk.core.status import ZombieException 

etp_exec_dispatcher_token = DispatcherToken()

def is_listlist_empty(l):
    """Returns true if l=[[],[], ...]
    :type l: a list of lists.
    """
    return not any(l)

class Etp(object):
    """Extended Tracer Packet"""

    def __init__(self, ntkd_status, time_tick_serializer, radar, maproute):

        self.ntkd_status = ntkd_status
        self.time_tick_serializer = time_tick_serializer
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        # ETP sequence number
        self.etp_seq_num = 0
        
        self.neigh.events.listen('NEIGH_NEW', self.etp_new_link)
        self.neigh.events.listen('COLLIDING_NEIGH_NEW', self.etp_new_link)
        self.neigh.events.listen('NEIGH_REM_CHGED', self.etp_changed_link)
        self.neigh.events.listen('NEIGH_DELETED', self.etp_dead_link)

        self.events = Event(['TIME_TICK', 'NET_COLLISION'])

        self.events.listen('TIME_TICK', self.neigh.check_needs_readvertise_local)
        # This should be placed in module radar. TODO refactor.

        self.remotable_funcs = [self.etp_exec,
                                self.etp_exec_udp]

    @microfunc(True)
    def etp_send_to_neigh(self, etp, neigh, current_netid, current_nip):
        """Sends the `etp' to neigh"""

        # Increment ETP sequence number
        self.etp_seq_num += 1
        # Append ETP sequence number
        etp = (etp[0], etp[1], etp[2], self.etp_seq_num)

        if current_netid != self.neigh.netid:
            logging.info('An ETP dropped because we changed network from ' +
                         str(current_netid) + ' to ' + 
                         str(self.neigh.netid) + '.')
            return
        if current_nip != self.maproute.me:
            logging.info('An ETP dropped because we changed NIP from ' +
                         str(current_nip) + ' to ' + 
                         str(self.maproute.me) + '.')
            return
        if neigh.netid == -1:
            logging.info('etp_send_to_neigh: An ETP dropped because the neighbour is hooking.')
            return
        logging.debug('Etp: sending to %s', str(neigh))

        # If an ETP does not reach a neighbour, we should retry, as long as
        # the neighbour is seen by the radar.
        try:
            self.call_etp_exec_udp(neigh, self.maproute.me, 
                                   self.neigh.netid, *etp)
            logging.info('Sent ETP to %s', str(neigh))
        except Exception, e:
            if isinstance(e, RPCError):
                logging.warning('Etp: sending to ' + str(neigh) + ' RPCError.')
            else:
                logging.warning('Etp: sending to ' + str(neigh) + ' got' +
                              ' Exception ' + str(e) + '.')

            # An ETP was to be sent to neighbour (ip, netid) covering
            # dests destinations. It failed.
            dests = []
            R, TPL, flag_of_interest, seq_num = etp
            for lvl in xrange(self.maproute.levels):
                for dst, rem, hops in R[lvl]:
                    dests.append((lvl, dst))
            self.fail_etp_send_to_neigh(neigh.ip, neigh.netid, dests, current_netid, current_nip)

    @microfunc(True)
    def fail_etp_send_to_neigh(self, ip, netid, dests, current_netid, current_nip):
        """An ETP was to be sent to neighbour (ip, netid) covering
           dests destinations. It failed.
           This method tries again iff (ip, netid) is still in
           our Neighbours' list.
           
           dests is a sequence of pairs (lvl, dst)
           """

        if current_netid != self.neigh.netid:
            logging.info('An ETP dropped because we changed network from ' +
                         str(current_netid) + ' to ' + 
                         str(self.neigh.netid) + '.')
            return
        if current_nip != self.maproute.me:
            logging.info('An ETP dropped because we changed NIP from ' +
                         str(current_nip) + ' to ' + 
                         str(self.maproute.me) + '.')
            return

        logging.warning('Etp: sending to ' + ip_to_str(ip) + ' in ' + str(netid) + ' failed.')
        # Check if X (ip, netid) is still in our Neighbours' list.
        neigh = self.neigh.key_to_neigh((ip, netid))
        if neigh:
            logging.warning('Etp: We try again since the neighbour should be still there.')
            # The neighbour X could be in another 
            # network. I must check.
            another_network = self.neigh.netid != netid

            # I prepare an ETP for X with empty TPL
            R = []
            for lvl in xrange(self.maproute.levels):
                R.append([])
            flag_of_interest=1
            ## The TPL starts with just myself.
            TPL = [[0, [[self.maproute.me[0], NullRem()]]]]

            # ∀v ∈ dests
            for lvl, dst in dests:
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    if another_network:
                        # A computes Bestᵗ (A → v)
                        best = routes_to_v.best_route()
                        # if Bestᵗ (A → v) > 0
                        if best is not None:
                            # new route Bestᵗ (A → v) has to be sent to X
                            R[lvl].append((dst, best.rem, best.hops_with_gw))
                    else:
                        # A computes Bestᵗ (A → X̃ → v)
                        X_lvl_id = self.maproute.routeneigh_get(neigh)
                        best = routes_to_v.best_route_without(X_lvl_id)
                        # if Bestᵗ (A → X̃ → v) > 0
                        if best is not None:
                            # new route Bestᵗ (A → X̃ → v) has to be sent to X
                            R[lvl].append((dst, best.rem, best.hops_with_gw))

            # We must add a route to ourself.
            for lvl in xrange(self.maproute.levels):
                R[lvl].append((self.maproute.me[lvl], NullRem(), []))

            etp = (R, TPL, flag_of_interest)
            self.etp_send_to_neigh(etp, neigh, current_netid, current_nip)

        else:
            logging.warning('Etp: We don\'t see the neighbour anymore. Ignore it.')

    def etp_dead_link(self, neigh, before_dead_link):
        """Builds and sends a new ETP for the dead link case."""

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, I must not react to this event.
            return

        logging.debug('Etp: etp_dead_link. neigh ' + str(neigh) + ', before_dead_link ' + str(before_dead_link))

        # Memorize current netid and nip because they might change. In this case the 
        # ETP should not be neither executed nor forwarded.
        current_netid = self.neigh.netid
        current_nip = self.maproute.me

        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        logging.debug('Etp: etp_dead_link. in_my_network ' + str(current_nr_list))
        # Prepare R of new ETP:
        # Note that R has to be created for each one of our neighbours.
        # That is, ∀w ∈ A*
        set_of_R = {}
        for nr in current_nr_list:
            set_of_R[nr] = []
            for lvl in xrange(self.maproute.levels):
                set_of_R[nr].append([])
        # step 1 of CLR (similar to a dead link case)
        # A computes rᵗ⁺¹A (→ B) = Rᵗ⁺¹ (AB)   It is now a DeadRem.
        self.maproute.routeneigh_del(neigh)
        # Now, ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                # step 2 of CLR
                # A computes Bestᵗ⁺¹ (A → v)
                # Since old routes (paths) have been deleted, each RouteNode.best_route is automatically up to date.
                # step 3 of CLR
                # if ΔBestᵗ⁺¹ (A → v) ≠ 0
                if (lvl, dst) in before_dead_link:
                    # At least one path to v existed before the dead link.
                    oldrem = before_dead_link[(lvl, dst)][0]
                    if routes_to_v.is_empty() or routes_to_v.best_route().rem != oldrem:
                        # ∀w ∈ A* | w ≠ B   (which is dead)
                        for w in set_of_R:
                            R = set_of_R[w]
                            w_lvl_id = self.maproute.routeneigh_get(w)
                            # new route Bestᵗ⁺¹ (A → w̃ → v) has to be sent to w
                            new_best_not_w = routes_to_v.best_route_without(w_lvl_id)
                            if new_best_not_w:
                                R[lvl].append((dst, new_best_not_w.rem, new_best_not_w.hops_with_gw))
                            else:
                                R[lvl].append((dst, DeadRem(), []))
        # Now, ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    best_to_v = routes_to_v.best_route()
                    xtime.sleep_during_hard_work(0)
                    # ∀w ∈ A* | w ≠ B   (which is dead)
                    for w in set_of_R:
                        R = set_of_R[w]
                        w_lvl_id = self.maproute.routeneigh_get(w)
                        # if w ∈ Bestᵗ (A → v) XOR w ∈ Bestᵗ⁺¹ (A → v)
                        previous_best_contained = before_dead_link[(lvl, dst)][1][w_lvl_id]
                        if best_to_v.contains(w_lvl_id) != previous_best_contained:
                            # new route Bestᵗ⁺¹ (A → w̃ → v) has to be sent to w
                            new_best_not_w = routes_to_v.best_route_without(w_lvl_id)
                            present = False
                            for rr in R[lvl]:
                                if rr[0] == dst:
                                    present = True
                            if not present:
                                R[lvl].append((dst, new_best_not_w.rem, new_best_not_w.hops_with_gw))

        flag_of_interest=1
        ## The TPL starts with just myself.
        TPL = [[0, [[self.maproute.me[0], NullRem()]]]]
        # ∀w ∈ A* | w ≠ B   (which is dead)
        for w in set_of_R:
            R = set_of_R[w]
            if any(R):
                etp = (R, TPL, flag_of_interest)
                self.etp_send_to_neigh(etp, w, current_netid, current_nip)

    def etp_new_link(self, neigh):
        """Builds and sends a new ETP for the new link case."""

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, I must not react to this event.
            return

        # Memorize current netid and nip because they might change. In this case the 
        # ETP should not be neither executed nor forwarded.
        current_netid = self.neigh.netid
        current_nip = self.maproute.me

        # The new neighbour could be in another 
        # network. I must check.
        another_network = self.neigh.netid != neigh.netid

        # A prepares an ETP for B with TPL=[A]
        R = []
        for lvl in xrange(self.maproute.levels):
            R.append([])
        flag_of_interest=1
        ## The TPL starts with just myself.
        TPL = [[0, [[self.maproute.me[0], NullRem()]]]]

        # ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    if another_network:
                        # A computes Bestᵗ (A → v)
                        best = routes_to_v.best_route()
                        # if Bestᵗ (A → v) > 0
                        if best is not None:
                            # new route Bestᵗ (A → v) has to be sent to B
                            R[lvl].append((dst, best.rem, best.hops_with_gw))
                    else:
                        # A computes Bestᵗ (A → B̃ → v)
                        B_lvl_id = self.maproute.routeneigh_get(neigh)
                        best = routes_to_v.best_route_without(B_lvl_id)
                        # if Bestᵗ (A → B̃ → v) > 0
                        if best is not None:
                            # new route Bestᵗ (A → B̃ → v) has to be sent to B
                            R[lvl].append((dst, best.rem, best.hops_with_gw))

        # We must add a route to ourself.
        for lvl in xrange(self.maproute.levels):
            R[lvl].append((self.maproute.me[lvl], NullRem(), []))

        etp = (R, TPL, flag_of_interest)
        self.etp_send_to_neigh(etp, neigh, current_netid, current_nip)

    def etp_changed_link(self, neigh, oldrem, before_changed_link):
        """Builds and sends a new ETP for the changed link case."""

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, I must not react to this event.
            return

        # Memorize current netid and nip because they might change. In this case the 
        # ETP should not be neither executed nor forwarded.
        current_netid = self.neigh.netid
        current_nip = self.maproute.me

        current_nr_list = self.neigh.neigh_list()
        # Prepare R of new ETP:
        # Note that R has to be created for each one of our neighbours.
        # That is, ∀w ∈ A*
        set_of_R = {}
        for nr in current_nr_list:
            set_of_R[nr] = []
            for lvl in xrange(self.maproute.levels):
                set_of_R[nr].append([])
        # step 1 of CLR
        # A computes rᵗ⁺¹A (→ B) = Rᵗ⁺¹ (AB)
        # It is already in neigh.rem
        # Now, ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                # step 2 of CLR
                # A computes Bestᵗ⁺¹ (A → v)
                # Since neigh.rem is updated, each RouteNode.best_route is automatically up to date.
                # We only need to emit signal that the routes are updated.
                self.maproute.route_signal_rem_changed(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    # step 3 of CLR
                    # if ΔBestᵗ⁺¹ (A → v) ≠ 0
                    oldrem = before_changed_link[(lvl, dst)][0]
                    if routes_to_v.best_route().rem != oldrem:
                        # ∀w ∈ A* | w ≠ B
                        for w in set_of_R:
                            if w is not neigh:
                                R = set_of_R[w]
                                w_lvl_id = self.maproute.routeneigh_get(w)
                                # new route Bestᵗ⁺¹ (A → w̃ → v) has to be sent to w
                                new_best_not_w = routes_to_v.best_route_without(w_lvl_id)
                                R[lvl].append((dst, new_best_not_w.rem, new_best_not_w.hops_with_gw))
        # Now, ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    # ∀w ∈ A*
                    for w in set_of_R:
                        R = set_of_R[w]
                        w_lvl_id = self.maproute.routeneigh_get(w)
                        # if w ∈ Bestᵗ (A → v) XOR w ∈ Bestᵗ⁺¹ (A → v)
                        previous_best_contained = before_changed_link[(lvl, dst)][1][w_lvl_id]
                        if best_to_v.contains(w_lvl_id) != previous_best_contained:
                            # new route Bestᵗ⁺¹ (A → w̃ → v) has to be sent to w
                            new_best_not_w = routes_to_v.best_route_without(w_lvl_id)
                            present = False
                            for rr in R[lvl]:
                                if rr[0] == dst:
                                    present = True
                            if not present:
                                R[lvl].append((dst, new_best_not_w.rem, new_best_not_w.hops_with_gw))

        flag_of_interest=1
        ## The TPL starts with just myself.
        TPL = [[0, [[self.maproute.me[0], NullRem()]]]]
        # ∀w ∈ A*
        for w in set_of_R:
            R = set_of_R[w]
            if any(R):
                etp = (R, TPL, flag_of_interest)
                self.etp_send_to_neigh(etp, w, current_netid, current_nip)

    @microfunc(True)
    def etp_exec(self, sender_nip, sender_netid, R, TPL, flag_of_interest, seq_num):

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        # If we are hooking we should not consider any ETP. But if we are waiting
        # for some ETP we must consider only the ones from our new network.
        # After the hook, on event HOOKED2, we'll readvertise to the outside neighbours.
        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            return
        if not self.ntkd_status.hooked:
            logging.debug('Still waiting ETP only from new network...')
            if sender_netid != self.neigh.netid:
                logging.debug('...this is not.')
                return
            else:
                logging.debug('...this is one of them.')

        # Memorize current netid and nip because they might change. In this case the 
        # ETP should not be neither executed nor forwarded.
        current_netid = self.neigh.netid
        current_nip = self.maproute.me

        gwnip = sender_nip
        gwip = self.maproute.nip_to_ip(gwnip)
        neigh = self.neigh.key_to_neigh((gwip, sender_netid))
        # Check if we have found the neigh, otherwise wait it.
        # We can, and really should, wait for long time, since we
        # are not preventing other tasklets from doing their work.
        # We just check at each interval, cause we could split or
        # rehook.
        timeout = xtime.time() + 300000
        while neigh is None:
            if xtime.time() > timeout:
                logging.info('ETP from (nip, netid) = ' + 
                             str((sender_nip, sender_netid)) + 
                             ' dropped: timeout.')
                return
            xtime.swait(50)
            neigh = self.neigh.key_to_neigh((gwip, sender_netid))

            # Implements "zombie" status
            if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')
            # If we are hooking we should not consider any ETP. But if we are waiting
            # for some ETP we must consider only the ones from our new network.
            # After the hook, on event HOOKED2, we'll readvertise to the outside neighbours.
            if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
                return
            if not self.ntkd_status.hooked:
                if sender_netid != self.neigh.netid:
                    logging.debug('Still waiting ETP only from new network...')
                    logging.debug('...this is not.')
                    return

        self.time_tick_serializer(self.serialized_etp_exec, (neigh, current_netid, current_nip, sender_nip, sender_netid, R, TPL, flag_of_interest, seq_num))

    def call_etp_exec_udp(self, neigh, sender_nip, sender_netid, R, TPL, 
                          flag_of_interest, seq_num):
        """Use BcastClient to call etp_exec"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'etp.etp_exec_udp', 
                            (sender_nip, sender_netid, R, TPL, 
                             flag_of_interest, seq_num))

    def etp_exec_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid, 
                     sender_nip, sender_netid, R, TPL, flag_of_interest, seq_num):
        """Returns the result of etp_exec to remote caller.
           caller_id is the random value generated by the caller 
           for this call.
            It is replied back to the LAN for the caller to recognize a 
            reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request 
            destinated to it.
           """
        if self.maproute.me == callee_nip and \
           self.neigh.netid == callee_netid:
            self.etp_exec(sender_nip, sender_netid, R, TPL, flag_of_interest, seq_num)
            # Since it is micro, I will reply None
            # An error here is ignorable.
            try:
                rpc.UDP_send_reply(_rpc_caller, caller_id, None)
            except:
                logging.debug("etp_exec_udp: Exception while replying. Ignore.")

    def serialized_etp_exec(self, neigh, current_netid, current_nip, sender_nip, sender_netid, R, TPL, flag_of_interest, seq_num):
        """Executes a received ETP

        sender_nip: sender ntk ip (see map.py)
        sender_netid: updated network id of the sender
        R  : the set of routes of the ETP
        TPL: the tracer packet of the path covered until now by this ETP.
             This TP may have covered different levels. In general, TPL
             is a list of blocks. Each block is a (lvl, TP) pair, where lvl is
             the level of the block and TP is the tracer packet composed
             during the transit in the level `lvl'.
             TP is a list of (hop, rem) pairs.
        flag_of_interest: a boolean
        seq_num: Sequence number of the ETP
        """

        try:
            etp_exec_dispatcher_token.executing = True

            # Initialize neigh.etp_seq_num if needed
            if neigh.etp_seq_num is None:
                neigh.etp_seq_num = [[0] * self.maproute.gsize] * self.maproute.levels

            # Implements "zombie" status
            if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

            # If we are hooking we should not consider any ETP. But if we are waiting
            # for some ETP we must consider only the ones from our new network.
            # After the hook, on event HOOKED2, we'll readvertise to the outside neighbours.
            if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
                return
            if not self.ntkd_status.hooked:
                logging.debug('Still waiting ETP only from new network...')
                if sender_netid != self.neigh.netid:
                    logging.debug('...this is not.')
                    return
                else:
                    logging.debug('...this is one of them.')

            # Ignore ETP from -1 ... that should not happen.
            if sender_netid == -1:
                # I raise exception just to give more visibility.
                raise Exception('ETP received from a node with netid = -1 '
                                '(not completely kooked).')

            # Before starting ETP execution,
            # check that situation is still valid.
            if current_netid != self.neigh.netid:
                logging.info('An ETP dropped because we changed network from ' +
                             str(current_netid) + ' to ' + 
                             str(self.neigh.netid) + '.')
                return
            if current_nip != self.maproute.me:
                logging.info('An ETP dropped because we changed NIP from ' +
                             str(current_nip) + ' to ' + 
                             str(self.maproute.me) + '.')
                return

            # This is a serialized method that passes from time t to time t+1.
            # As such, it will:
            #  .evaluate (t-1)-like stuff (node_nb, ...)
            #  .update our memorized paths
            #  .evaluate (t)-like stuff (node_nb, ...)
            #  .emit TIME_TICK event

            ## evaluate current node_nb.
            old_node_nb = self.maproute.node_nb[:]

            ## Calculate the size of my network.
            def add(a,b):return a+b
            mynetsz = reduce(add, self.maproute.node_nb)
            logging.debug('Before execution of ETP, my network size is ' + str(mynetsz-3) + '.')

            ## Calculate the size of the network as seen by my neighbour.
            # This can be useful if there is a network collision.
            ngnetsz = reduce(add, map(len, R))
            logging.debug('As seen by the sender, the ETP covers ' + str(ngnetsz) + ' destinations.')

            before_etp_exec = {}
            current_nr_list = self.neigh.neigh_list(in_my_network=True)
            # Takes note of the current situation, the bits of information that
            # we'll need after this update.
            # ∀v ∈ V
            for lvl in xrange(self.maproute.levels):
                xtime.sleep_during_hard_work(10)
                for dst in xrange(self.maproute.gsize):
                    routes_to_v = self.maproute.node_get(lvl, dst)
                    if not routes_to_v.is_empty():
                        xtime.sleep_during_hard_work(0)
                        # D computes rem(Bestᵗ (D → v))
                        old_best = routes_to_v.best_route()
                        before_etp_exec[(lvl, dst)] = (old_best.rem, {})
                        # ∀w ∈ D*
                        for w in current_nr_list:
                            w_lvl_id = self.maproute.routeneigh_get(w)
                            # D checks wether "w ∈ Bestᵗ (D → v)"
                            before_etp_exec[(lvl, dst)][1][w_lvl_id] = old_best.contains(w_lvl_id)

            gwnip = sender_nip
            gwip = self.maproute.nip_to_ip(gwnip)

            level = self.maproute.nip_cmp(self.maproute.me, gwnip)
            
            ## Purify map portion R from destinations that are not in
            ## a common gnode
            for lvl in reversed(xrange(level)):
                R[lvl] = []

            ## Purify map portion R from hops that are not in
            ## a common gnode
            for R_lvl in R:
                for id in xrange(len(R_lvl)):
                    dst, rem, hops = R_lvl[id]
                    hops = self.maproute.list_lvl_id_from_nip(hops, sender_nip)
                    R_lvl[id] = (dst, rem, hops)

            ## Group rule
            for block in TPL:
                lvl = block[0] # the level of the block
                if lvl < level:
                    block[0] = level                     
                    blockrem = sum([rem for hop, rem in block[1]], 
                                   NullRem())
                    block[1] = [[gwnip[level], blockrem]]

            ### Collapse blocks of the same level
            #Note: we're assuming the two blocks with the same level are one after
            #      another.
            TPL2 = [TPL[0]]

            for block in TPL[1:]:
                if block[0] == TPL2[-1][0]:
                    TPL2[-1][1]+=block[1]
                else:
                    TPL2.append(block)
            TPL = TPL2
            ###

            ### Remove dups
            def remove_contiguos_dups_in_TP(L):
                L2 = []
                prec = [None, NullRem()]
                for x in L:
                    if x[0] != prec[0]:
                        prec = x
                        L2.append(x)
                    else:
                        prec[1] += x[1]
                return L2

            for block in TPL:
                block[1] = remove_contiguos_dups_in_TP(block[1])
            ###

            ## ATP rule
            for lvl, pairs in TPL:
                if self.maproute.me[lvl] in [hop for hop, rem in pairs]:
                    logging.debug('ETP received: Executing: Ignore ETP because '
                                  'of Acyclic Rule.')
                    return    # drop the pkt
            ##

            ## The rem of the first block is useless.
            TPL[0][1][0][1] = NullRem()
            ##

            # Before checking collision,
            # check that situation is still valid.
            if current_netid != self.neigh.netid:
                logging.info('An ETP dropped because we changed network from ' +
                             str(current_netid) + ' to ' + 
                             str(self.neigh.netid) + '.')
                return
            if current_nip != self.maproute.me:
                logging.info('An ETP dropped because we changed NIP from ' +
                             str(current_nip) + ' to ' + 
                             str(self.maproute.me) + '.')
                return
            if sender_netid == -1:
                logging.info('etp_exec: An ETP dropped because the neighbour is hooking.')
                return

            logging.debug('Translated ETP from %s', ip_to_str(gwip))
            logging.debug('R: ' + str(R))
            logging.debug('TPL: ' + str(TPL))

            ## Collision check
            colliding, R = self.collision_check(sender_netid, R, mynetsz, ngnetsz)
            if colliding:
                # Collision detected. Let's rehook with the others' netid.
                # ... in another tasklet.
                self.collision_rehook(sender_netid)
                return # drop the packet

            if not any(R):
                return # drop the packet
            ##

            gwrem = neigh.rem

            xtime.swait(10)

            # For each one of our neighbours, we prepare a ETP
            #  with the TPL received *and* a new ETP, with just
            #  ourself in the TPL.
            new_TPL = [[0, [[self.maproute.me[0], NullRem()]]]]
            forwarded_TPL = TPL[:]
            if forwarded_TPL[-1][0] != 0: 
                # The last block isn't of level 0. Let's add a new block
                TP = [[self.maproute.me[0], gwrem]] 
                forwarded_TPL.append([0, TP])
            else:
                # The last block is of level 0. We can append our ID
                forwarded_TPL[-1][1].append([self.maproute.me[0], gwrem])
            set_of_forwarded_R = {}
            set_of_new_R = {}
            # ∀w ∈ D*   (I am D)
            for nr in current_nr_list:
                set_of_forwarded_R[nr] = []
                set_of_new_R[nr] = []
                for lvl in xrange(self.maproute.levels):
                    set_of_forwarded_R[nr].append([])
                    set_of_new_R[nr].append([])

            # ∀r ∈ ETP
            for lvl in xrange(self.maproute.levels):
                for dst, rem, hops in R[lvl]:
                  # Check if informations for this destination through this gateway is out-of-date
                  if seq_num > neigh.etp_seq_num[lvl][dst]:
                    neigh.etp_seq_num[lvl][dst] = seq_num
                    logging.debug('Will process "r" received in a ETP by ' + str(neigh) + ': lvl, dst, rem, hops = ' + str((lvl, dst, rem, hops)))
                    # rem is Bestᵗ(C → v) with v ∈ V
                    #   where C is our neighbour (neigh)
                    #   and v = (lvl, dst)
                    # For proposition 3.2 hops should not contain ourself.
                    # D updates rᵗ⁺¹D (C → v) = Bestᵗ (C → v)
                    routes_to_v = self.maproute.node_get(lvl, dst)
                    logging.debug('before update:')
                    logging.debug('  # routes = ' + str(len(routes_to_v.routes)))
                    prev_best = routes_to_v.best_route()
                    if prev_best:
                        logging.debug('  Bestᵗ (D → v) = ' + str(prev_best.rem) + ' via ' + str(prev_best.gw))

                    # Before processing a path,
                    # check that situation is still valid.
                    if current_netid != self.neigh.netid:
                        logging.info('An ETP dropped because we changed network from ' +
                                     str(current_netid) + ' to ' + 
                                     str(self.neigh.netid) + '.')
                        return
                    if current_nip != self.maproute.me:
                        logging.info('An ETP dropped because we changed NIP from ' +
                                     str(current_nip) + ' to ' + 
                                     str(self.maproute.me) + '.')
                        return

                    self.maproute.update_route_by_gw((lvl, dst), neigh, rem, hops)
                    # D computes Bestᵗ⁺¹ (D → v)
                    best = routes_to_v.best_route()
                    logging.debug('after update:')
                    logging.debug('  # routes = ' + str(len(routes_to_v.routes)))
                    if best:
                        logging.debug('  Bestᵗ⁺¹ (D → v) = ' + str(best.rem) + ' via ' + str(best.gw))
                    # ∀w ∈ D* | w ≠ C
                    for w in current_nr_list:
                        if w is not neigh:
                            w_lvl_id = self.maproute.routeneigh_get(w)
                            # if ΔBestᵗ⁺¹ (D → v) ≠ 0
                            # Retrieve Bestᵗ (D → v)
                            prev_best_rem = DeadRem()
                            if (lvl, dst) in before_etp_exec:
                                prev_best_rem = before_etp_exec[(lvl, dst)][0]
                            # Retrieve Bestᵗ⁺¹ (D → v)
                            cur_best_rem = DeadRem()
                            if best:
                                cur_best_rem = best.rem
                            if cur_best_rem != prev_best_rem:
                                logging.debug('Will send ETP received by ' + str(neigh) + ' to ' + str(w_lvl_id) + ' for ' + str((lvl, dst)) + ' because: cur_best_rem != prev_best_rem')
                                logging.debug('cur_best_rem = ' + str(cur_best_rem))
                                logging.debug('prev_best_rem = ' + str(prev_best_rem))
                                # new route Bestᵗ⁺¹ (D → w̃ → v) has to be sent to w (con il TPL allungato)
                                best_without = routes_to_v.best_route_without(w_lvl_id)
                                if best_without:
                                    logging.debug('Will send ' + str(best_without.hops_with_gw) + ' to ' + str(w_lvl_id) + ' appending myself to TPL.')
                                    set_of_forwarded_R[w][lvl].append((dst, best_without.rem, best_without.hops_with_gw))
                                else:
                                    logging.debug('Will send DeadRem appending myself to TPL.')
                                    set_of_forwarded_R[w][lvl].append((dst, DeadRem(), []))
                  else:
                    logging.debug('Will NOT process "r" received in a ETP by ' + str(neigh) + ' for (' + str(lvl) + ', ' + str(dst) + ') because it is out-of-date.')

            # Now, ∀v ∈ V
            for lvl in xrange(self.maproute.levels):
                xtime.sleep_during_hard_work(10)
                for dst in xrange(self.maproute.gsize):
                    routes_to_v = self.maproute.node_get(lvl, dst)
                    best = routes_to_v.best_route()
                    # Retrieve Bestᵗ (D → v)
                    prev_best_rem = DeadRem()
                    if (lvl, dst) in before_etp_exec:
                        prev_best_rem = before_etp_exec[(lvl, dst)][0]
                    # Retrieve Bestᵗ⁺¹ (D → v)
                    cur_best_rem = DeadRem()
                    if best:
                        cur_best_rem = best.rem
                    # We can skip nodes that we never knew.
                    if prev_best_rem == DeadRem() and cur_best_rem == DeadRem():
                        pass
                    else:
                        logging.debug('NODO ' + str((lvl, dst)))
                        # ∀w ∈ D*
                        for w in current_nr_list:
                            w_lvl_id = self.maproute.routeneigh_get(w)
                            best_has_changed = cur_best_rem != prev_best_rem
                            w_in_previous_best = False
                            if (lvl, dst) in before_etp_exec:
                                w_in_previous_best = before_etp_exec[(lvl, dst)][1][w_lvl_id]
                            w_in_current_best = False
                            if best:
                                w_in_current_best = best.contains(w_lvl_id)
                            w_in_has_changed = w_in_previous_best != w_in_current_best
                            # if ΔBestᵗ⁺¹ (D → v) ≠ 0 OR (w ∈ Bestᵗ⁺¹ (D → v) XOR w ∈ Bestᵗ (D → v))
                            if best_has_changed or w_in_has_changed:
                                logging.debug('Will send ETP received by ' + str(neigh) + ' to ' + str(w_lvl_id) + ' for ' + str((lvl, dst)) + ' because: best_has_changed or w_in_has_changed')
                                logging.debug('cur_best_rem = ' + str(cur_best_rem))
                                logging.debug('prev_best_rem = ' + str(prev_best_rem))
                                logging.debug('w_in_current_best = ' + str(w_in_current_best))
                                logging.debug('w_in_previous_best = ' + str(w_in_previous_best))
                                w_in_tpl = False
                                w_lvl, w_id = w_lvl_id
                                for block in forwarded_TPL:
                                    if w_lvl == block[0]:
                                        for pair in block[1]:
                                            if w_id == pair[0]:
                                                w_in_tpl = True
                                            if w_in_tpl:
                                                break
                                    if w_in_tpl:
                                        break
                                # if w ∈ TPL allungato
                                if w_in_tpl:
                                    # new route Bestᵗ⁺¹ (D → w̃ → v) has to be sent to w (con il TPL nuovo)
                                    best_without = routes_to_v.best_route_without(w_lvl_id)
                                    if best_without:
                                        logging.debug('Will send ' + str(best_without.hops_with_gw) + ' to ' + str(w_lvl_id) + ' appending myself to TPL.')
                                        set_of_new_R[w][lvl].append((dst, best_without.rem, best_without.hops_with_gw))
                                    else:
                                        logging.debug('Will send DeadRem appending myself to TPL.')
                                        set_of_new_R[w][lvl].append((dst, DeadRem(), []))
                                else:
                                    # new route Bestᵗ⁺¹ (D → w̃ → v) has to be sent to w (con il TPL allungato)
                                    best_without = routes_to_v.best_route_without(w_lvl_id)
                                    if best_without:
                                        logging.debug('Will send ' + str(best_without.hops_with_gw) + ' to ' + str(w_lvl_id) + ' in a new ETP.')
                                        set_of_forwarded_R[w][lvl].append((dst, best_without.rem, best_without.hops_with_gw))
                                    else:
                                        logging.debug('Will send DeadRem in a new ETP.')
                                        set_of_forwarded_R[w][lvl].append((dst, DeadRem(), []))

            flag_of_interest=1
            # ∀w ∈ D*
            for w in set_of_forwarded_R:
                R = set_of_forwarded_R[w]
                if any(R):
                    etp = (R, forwarded_TPL, flag_of_interest)
                    self.etp_send_to_neigh(etp, w, current_netid, current_nip)
            for w in set_of_new_R:
                R = set_of_new_R[w]
                if any(R):
                    etp = (R, new_TPL, flag_of_interest)
                    self.etp_send_to_neigh(etp, w, current_netid, current_nip)

            logging.info('ETP executed.')

            ## Calculate the size of my network.
            mynetsz = reduce(add, self.maproute.node_nb)
            logging.debug('After execution of ETP, my network size is ' + str(mynetsz-3) + '.')

            ## evaluate current node_nb.
            cur_node_nb = self.maproute.node_nb[:]

            self.events.send('TIME_TICK', (old_node_nb, cur_node_nb))
        finally:
            etp_exec_dispatcher_token.executing = False

    def collision_check(self, neigh_netid, R, mynetsz, ngnetsz):
        """ Checks if we are colliding with the network of `neigh'.

            It returns True if we are colliding and we are going to rehook.
            !NOTE! the set R will be modified: all the colliding routes will
            be removed.
        """
        
        logging.debug("Etp: collision check: my netid %d and neighbour's "
                      "netid %d", self.neigh.netid, neigh_netid)

        if neigh_netid == self.neigh.netid:
            return (False, R) # all ok

        # uhm... we are in different networks
        logging.info('Detected Network Collision')

        # If we are already hooking (going to hook, doing hook or just hooked
        # and still waiting for some ETP) we won't consider this collision.
        # After the hook, on event HOOKED2, we'll readvertise.
        if not self.ntkd_status.hooked:
            # We are already hooking. Just ignore this ETP.
            ### Remove all routes from R
            R = [ [] for lvl in xrange(self.maproute.levels) ]
            logging.info("We are already hooking. Just ignore this ETP.")
            return (False, R)

        logging.info('My network size = ' + str(mynetsz-3))
        logging.info('Their network size = ' + str(ngnetsz-3))

        if mynetsz > ngnetsz or                                         \
            (mynetsz == ngnetsz and self.neigh.netid > neigh_netid):
            # We are the bigger net.
            # We cannot use routes from R, since this gateway is going 
            # to re-hook.

            ### Remove all routes from R
            R = [ [] for lvl in xrange(self.maproute.levels) ]
            ###
            
            logging.debug("Etp: collision check: just remove all routes "
                          "from R")
            return (False, R)
        ##

        # We are the smaller net.
        # In any case, we have to re-hook.
        # TODO Find a way, if possible, to not re-hook. We need however to 
        # contact the coordinator node for our new place in the new network.
        # TODO Question: would it be really useful to not rehook? That is,
        # to maintain the same IP in the new network?
        logging.debug('Etp: we are the smaller net, we must rehook now.')
        return (True, R)

    @microfunc(True)
    def collision_rehook(self, neigh_netid):
        """ We are colliding and we are going to rehook.
        """
        
        logging.debug("Etp: collision detected: rehook to netid %d.",
                      neigh_netid)
        the_others = self.neigh.neigh_list(in_this_netid=neigh_netid)
        # If we want to rehook with the_others, we have to make sure 
        # there is someone! We have to wait. And we could fail anyway.
        # In that case abort the processing of this request.
        timeout = xtime.time() + 16000
        while not any(the_others):
            if xtime.time() > timeout:
                logging.info('Rehooking to netid = ' + 
                             str(neigh_netid) + 
                             ' aborted: timeout.')
                return
            xtime.swait(50)
            the_others = self.neigh.neigh_list(in_this_netid=
                                               neigh_netid)
        # If some other thing has caused a rehook, never mind.
        if self.ntkd_status.hooked:
            self.ntkd_status.gonna_hook = True
            self.events.send('NET_COLLISION', 
                             (the_others,)
                            )

