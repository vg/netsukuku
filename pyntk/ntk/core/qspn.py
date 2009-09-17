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

from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block
from ntk.lib.rpc import RPCError
import ntk.lib.rpc as rpc
from ntk.lib.event import Event, apply_wakeup_on_event
from ntk.network.inet import ip_to_str
from ntk.core.route import NullRem, DeadRem
import ntk.wrap.xtime as xtime
import time


def is_listlist_empty(l):
    """Returns true if l=[[],[], ...]
    :type l: a list of lists.
    """
    return not any(l)

class Etp(object):
    """Extended Tracer Packet"""

    def __init__(self, ntkd, radar, maproute):
        self.ntkd = ntkd
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        
        self.neigh.events.listen('NEIGH_NEW', self.etp_new_link)
        self.neigh.events.listen('NEIGH_REM_CHGED', self.etp_changed_link)
        self.neigh.events.listen('NEIGH_DELETED', self.etp_dead_link)

        self.events = Event(['ETP_EXECUTED', 'NET_COLLISION'])

        self.remotable_funcs = [self.etp_exec,
                                self.etp_exec_udp,
                                self.reply_etp_exec_udp]

    def etp_send_to_neigh(self, etp, neigh):
        """Sends the `etp' to neigh"""

        logging.debug('Etp: sending to %s', ip_to_str(neigh.ip))
        try:
            self.call_etp_exec_udp(neigh, self.maproute.me, self.ntkd.neighbour.netid, *etp)
            logging.info('Sent ETP to %s', ip_to_str(neigh.ip))
            # RPCErrors may arise for many reasons. We should not care.
        except RPCError:
            logging.debug('Etp: sending to %s RPCError. We ignore it.', ip_to_str(neigh.ip))
        else:
            logging.debug('Etp: sending to %s done.', ip_to_str(neigh.ip))

    @microfunc(True)
    def etp_dead_link(self, neigh):
        """Builds and sends a new ETP for the dead link case."""

        # If the neighbour was not in my network, I must do nothing.
        if not self.neigh.is_neigh_in_my_network(neigh):
            return

        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        logging.debug('QSPN: death of %s: update my map.', ip_to_str(neigh.ip))
        
        ## Create R
        # Note that R has to be created for each one of our neighbours. It must
        # contain all the routes through the dead gateway that each one of our neighbours
        # knows through us. So we must evaluate the best routes *excluding neigh x* that
        # pass through the dead gateway.
        def gw_is_neigh((dst, gw, rem, hops)):
            return gw.id == neigh.id
        set_of_R = {}
        for nr in current_nr_list:
            if nr.id != neigh.id:
                # It's a tough work! Be kind to other tasks.
                xtime.swait(10)
                set_of_R[nr.id] = self.maproute.bestroutes_get(f=gw_is_neigh, exclude_gw_ids=[nr.id])
        ##

        ## Update the map
        xtime.swait(10)
        self.maproute.routeneigh_del(neigh)
        ##

        while self.ntkd.neighbour.netid == -1:
            # I'm not ready to interact.
            time.sleep(0.001)
            micro_block()

        logging.debug('QSPN: death of %s: prepare the ETP', ip_to_str(neigh.ip))
        
        ## Prepare common part of the ETPs for the neighbours

        flag_of_interest=1
        TP = [[self.maproute.me[0], NullRem()]]    # Tracer Packet included in
        block_lvl = 0                           # the first block of the ETP
        ##

        # Through which devs did I see the defunct?
        devs_to_neigh = [dev for dev in neigh.devs.keys()]
        ## Forward the ETP to the neighbours
        for nr in current_nr_list:
            if nr.id != neigh.id:
                # Did this neighbour (nr) see the now defunct (neigh)
                # straightly? If so, we must NOT forward.
                devs_to_nr_and_neigh = [dev 
                                        for dev in nr.devs.keys()
                                        if dev in devs_to_neigh
                                       ]
                if not devs_to_nr_and_neigh:
                    # We must forward.

                    if is_listlist_empty(set_of_R[nr.id]):
                        # R is empty, that is we don't have routes passing by `gw'.
                        # Therefore, nothing to do for this neighbour.
                        continue

                    # Through which devs do I see this neighbour (nr)?
                    devs_to_nr = [dev for dev in nr.devs.keys()]
                    # Which neighbours do I see through those devs too?
                    common_devs_neighbours_to_nr = []
                    for nr2 in current_nr_list:
                        if nr2.id != nr.id:
                            devs_to_nr2_and_nr = [dev 
                                                  for dev in nr2.devs.keys()
                                                  if dev in devs_to_nr
                                                 ]
                            if devs_to_nr2_and_nr:
                                common_devs_neighbours_to_nr.append(nr2.id)
                    # We exclude them from research of bestroutes.
                    exclude_gw_ids = [nr.id] + common_devs_neighbours_to_nr

                    ## Create R2
                    def rem_or_none(r):
                        if r is not None:
                            return r.rem
                        return DeadRem()
                    R2 = [
                          [ (dst,
                             rem_or_none(
                                self.maproute.node_get(lvl, dst).best_route(exclude_gw_ids=exclude_gw_ids)
                                ),
                             hops+[self.maproute.nip_to_ip(self.maproute.me)]
                            ) for (dst,gw,rem,hops) in set_of_R[nr.id][lvl]
                          ] for lvl in xrange(self.maproute.levels)
                         ]
                    xtime.swait(10)
                    ##

                    etp = (R2, [[block_lvl, TP]], flag_of_interest)
                    logging.info('Sending ETP for a dead neighbour.')
                    self.etp_send_to_neigh(etp, nr)
        ##

    @microfunc(True)
    def etp_new_link(self, neigh):
        """Builds and sends a new ETP for the new link case."""

        while self.ntkd.neighbour.netid == -1:
            # I'm not ready to interact.
            time.sleep(0.001)
            micro_block()

        logging.debug('QSPN: new link %s: prepare the ETP', ip_to_str(neigh.ip))
        current_nr_list = self.neigh.neigh_list()

        # Through which devs do I see the new neighbour?
        devs_to_neigh = [dev for dev in neigh.devs.keys()]
        # Which neighbours do I see through those devs too?
        common_devs_neighbours_to_neigh = []
        for nr in current_nr_list:
            if nr.id != neigh.id:
                devs_to_nr_and_neigh = [dev 
                                        for dev in nr.devs.keys()
                                        if dev in devs_to_neigh
                                       ]
                if devs_to_nr_and_neigh:
                    common_devs_neighbours_to_neigh.append(nr.id)

        ## Create R
        exclude_gw_ids = [neigh.id] + common_devs_neighbours_to_neigh
        R = self.maproute.bestroutes_get(exclude_gw_ids=exclude_gw_ids)
        xtime.swait(10)

        # We must add a route to ourself.
        for lvl in xrange(self.maproute.levels):
            R[lvl].append((self.maproute.me[lvl], -1, NullRem(), []))

        def takeoff_gw((dst, gw, rem, hops)):
                return (dst, rem, hops)

        def takeoff_gw_lvl(L):
            return map(takeoff_gw, L)

        R = map(takeoff_gw_lvl, R)
        ##

        ## Send the ETP to `neigh'
        flag_of_interest=1
        TP = [[self.maproute.me[0], NullRem()]]
        etp = (R, [[0, TP]], flag_of_interest)
        logging.info('Sending ETP for a new neighbour.')
        self.etp_send_to_neigh(etp, neigh)
        ##

    @microfunc(True)
    def etp_changed_link(self, neigh, oldrem):
        """Builds and sends a new ETP for the changed link case."""

        # If the neighbour was not in my network, I must do nothing.
        if not self.neigh.is_neigh_in_my_network(neigh):
            return

        current_nr_list = self.neigh.neigh_list()
        logging.debug('QSPN: changed %s: update my map', ip_to_str(neigh.ip))
        
        ## Update the map
        self.maproute.routeneigh_rem(neigh, oldrem)
        ##

        while self.ntkd.neighbour.netid == -1:
            # I'm not ready to interact.
            time.sleep(0.001)
            micro_block()

        logging.debug('QSPN: changed %s: prepare the ETP', ip_to_str(neigh.ip))

        # We must evaluate *all* the routes that
        # pass through the changed-rem gateway.
        exclude_gw_ids = [nr.id for nr in current_nr_list if nr.id != neigh.id]
        R = self.maproute.bestroutes_get(exclude_gw_ids=exclude_gw_ids)
        xtime.swait(10)
        def takeoff_gw((dst, gw, rem, hops)):
                return (dst, rem, hops)
        def takeoff_gw_lvl(L):
                return map(takeoff_gw, L)
        R=map(takeoff_gw_lvl, R)
        ##

        flag_of_interest=1
        TPL = [[0, [[self.maproute.me[0], NullRem()]]]]
        ## TODO Add the correct part of TPL to include the neigh, because
        ##       if the ETP reaches the neigh itself, then it must stop for
        ##       the acyclic rule.

        logging.info('Forwarding ETP for a changed-rem neighbour.')
        self.etp_forward_referring_to_neigh(R, TPL, flag_of_interest, neigh)

    def call_etp_exec_udp(self, neigh, sender_nip, sender_netid, R, TPL, flag_of_interest):
        """Use BcastClient to call etp_exec"""
        devs = [neigh.bestdev[0]]
        nip = self.ntkd.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'etp.etp_exec_udp', (sender_nip, sender_netid, R, TPL, flag_of_interest))

    def etp_exec_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid, sender_nip, sender_netid, R, TPL, flag_of_interest):
        """Returns the result of etp_exec to remote caller.
           caller_id is the random value generated by the caller for this call.
            It is replied back to the LAN for the caller to recognize a reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated to it.
           """
        if self.maproute.me == callee_nip and self.neigh.netid == callee_netid:
            self.etp_exec(sender_nip, sender_netid, R, TPL, flag_of_interest)
            # Since it is micro, I will reply None
            rpc.UDP_send_reply(_rpc_caller, caller_id, 'etp.reply_etp_exec_udp', None)

    def reply_etp_exec_udp(self, _rpc_caller, caller_id, ret):
        """Receives reply from etp_exec_udp."""
        rpc.UDP_got_reply(_rpc_caller, caller_id, ret)

    @microfunc()
    def etp_exec(self, sender_nip, sender_netid, R, TPL, flag_of_interest):
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
        """

        logging.info('Received ETP from (nip, netid) = ' + str((sender_nip, sender_netid)))
        gwnip = sender_nip
        gwip = self.maproute.nip_to_ip(gwnip)
        # TODO remove:
        ## update our neighbour's netid in our netid_table
        #self.neigh.set_netid(gwip, sender_netid)
        neigh = self.neigh.key_to_neigh((gwip, sender_netid))
        
        # check if we have found the neigh, otherwise wait it
        timeout = xtime.time() + 6000
        while neigh is None:
            if xtime.time() > timeout:
                logging.info('ETP dropped: timeout.')
                return
            xtime.swait(50)
            neigh = self.neigh.key_to_neigh((gwip, sender_netid))

        gw_id = neigh.id
        gwrem = neigh.rem

        level = self.maproute.nip_cmp(self.maproute.me, gwnip)
        
        ## Purify map portion R
        for lvl in reversed(xrange(level)):
            R[lvl] = []

        ## Group rule
        for block in TPL:
                lvl = block[0] # the level of the block
                if lvl < level:
                        block[0] = level                     
                        blockrem = sum([rem for hop, rem in block[1]], NullRem())
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

        ##

        ## ATP rule
        for lvl, pairs in TPL:
            if self.maproute.me[lvl] in [hop for hop, rem in pairs]:
                logging.debug('ETP received: Executing: Ignore ETP because of Acyclic Rule.')
                return    # drop the pkt
        ##

        ## The rem of the first block is useless.
        TPL[0][1][0][1] = NullRem()
        ##

        logging.debug('Translated ETP from %s', ip_to_str(neigh.ip))
        logging.debug('R: ' + str(R))
        logging.debug('TPL: ' + str(TPL))
        
        ## Collision check
        colliding, R = self.collision_check(gwnip, neigh, R)
        if colliding:
                # collision detected. rehook.
                self.events.send('NET_COLLISION', 
                                 (self.neigh.neigh_list(in_this_netid=neigh.netid),)
                                )
                return # drop the packet
        ##

        xtime.swait(10)

        old_node_nb = self.maproute.node_nb[:]

        ### Update the map from the TPL
        #tprem=gwrem
        #TPL_is_interesting = False
        #for lvl, pairs in reversed(TPL):
        #    for dst, rem in reversed(pairs):
        #        xtime.swait(10)
        #        logging.debug('ETP received: Executing: TPL has info about this node:')
        #        logging.debug('    %s' % str((lvl, dst, gw, tprem)))
        #        if self.maproute.route_change(lvl, dst, gw, tprem):
        #            logging.debug('    Info is interesting. TPL is interesting. Map updated.')
        #            TPL_is_interesting = True
        #        tprem+=rem
        ###

        ## Update the map from R
        for lvl in xrange(self.maproute.levels):
            to_remove = []
            for dst, rem, hops in R[lvl]:
                xtime.swait(10)
                logging.debug('ETP received: Executing: R has info about this node:')
                logging.debug('    %s' % str((lvl, dst, gw_id, rem, len(hops))))
                if self.maproute.route_change(lvl, dst, neigh, rem, hops):
                    logging.debug('    Info is interesting. Map updated.')
                else:
                    to_remove.append((dst, rem, hops))
                    logging.debug('    Info is not interesting. Discarded.')
            for dst, rem, hops in to_remove:
                R[lvl].remove((dst, rem, hops))
        ##

        ## If I'm going to forward, I have to append myself.
        if TPL[-1][0] != 0: 
            # The last block isn't of level 0. Let's add a new block
            TP = [[self.maproute.me[0], gwrem]] 
            TPL.append([0, TP])
        else:
            # The last block is of level 0. We can append our ID
            TPL[-1][1].append([self.maproute.me[0], gwrem])
        ##

        self.etp_forward_referring_to_neigh(R, TPL, flag_of_interest, neigh)
        logging.info('ETP executed.')

        self.events.send('ETP_EXECUTED', (old_node_nb, self.maproute.node_nb[:]))

    def etp_forward_referring_to_neigh(self, R, TPL, flag_of_interest, neigh):
        """Forwards, when interesting, to all other neighbour info about neigh"""

        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        # Through which devs do I see the referree?
        devs_to_neigh = [dev for dev in neigh.devs.keys()]

        for nr in current_nr_list:
            if nr.id != neigh.id:
                # Does this neighbour (nr) see referree neighbour (neigh)
                # straightly? If so, we must NOT forward.
                devs_to_nr_and_neigh = [dev 
                                        for dev in nr.devs.keys()
                                        if dev in devs_to_neigh
                                       ]
                if not devs_to_nr_and_neigh:
                    # We must forward.
                    # Through which devs do I see this neighbour (nr)?
                    devs_to_nr = [dev for dev in nr.devs.keys()]
                    # Which neighbours do I see through those devs too?
                    common_devs_neighbours_to_nr = []
                    for nr2 in current_nr_list:
                        if nr2.id != nr.id:
                            devs_to_nr2_and_nr = [dev 
                                                  for dev in nr2.devs.keys()
                                                  if dev in devs_to_nr
                                                 ]
                            if devs_to_nr2_and_nr:
                                common_devs_neighbours_to_nr.append(nr2.id)
                    # We exclude them from research of bestroutes.
                    exclude_gw_ids = [nr.id] + common_devs_neighbours_to_nr
                    # Evaluate only once this set, which then we use twice:
                    xtime.swait(10)
                    best_routes_of_R = dict( [ ((lvl, dst), r)
                                               for lvl in xrange(self.maproute.levels)
                                                   for dst, rem, hops in R[lvl]
                                                       for r in [self.maproute.node_get(lvl, dst).best_route(exclude_gw_ids=exclude_gw_ids)]
                                             ] )
                    ## S
                    def nr_doesnt_care(lvl, dst, r):
                        # If destination is me (it happens) then nr does not care.
                        if self.maproute.me[lvl] == dst: return True
                        # If best route (except for nr) is none, then nr cares.
                        if r is None: return False 
                        # If best route (except for nr) is neigh, then nr cares.
                        # Else it does not.
                        return r.gw.id != neigh.id
                    # If step 5 is omitted we don't need the Rem in S.
                    S = [ [ (dst, None)
                            for lvl, dst in best_routes_of_R.keys()
                                if lvl == l
                                    for r in [best_routes_of_R[(lvl, dst)]]
                                        if nr_doesnt_care(lvl, dst, r)
                          ] for l in xrange(self.maproute.levels) ]

                    #--
                    # Step 5 omitted, see qspn.pdf, 4.1 Extended Tracer Packet:
                    # """If the property (g) holds, then the step 5 can be omitted..."""
                    #--
                    #if flag_of_interest:
                    #       if not is_listlist_empty(S):
                    #
                    #               Sflag_of_interest=0
                    #               TP = [(self.maproute.me[0], NullRem())]
                    #               etp = (S, [(0, TP)], Sflag_of_interest)
                    #               self.etp_send_to_neigh(etp, neigh)
                    ##

                    ## R2
                    xtime.swait(10)
                    def rem_or_none(r):
                        if r is not None:
                            return r.rem
                        return DeadRem()
                    def hops_or_none(r):
                        if r is not None:
                            return r.hops + [self.maproute.nip_to_ip(self.maproute.me)]
                        return []
                    R2 = [ [ (dst, rem_or_none(r), hops_or_none(r))
                            for lvl, dst in best_routes_of_R.keys()
                                if lvl == l
                                    for r in [best_routes_of_R[(lvl, dst)]]
                                        if dst not in [d for d, r2 in S[lvl]]
                          ] for l in xrange(self.maproute.levels) ]
                    ##

                    if not is_listlist_empty(R2):
                        etp = (R2, TPL, flag_of_interest)
                        logging.info('Sending ETP.')
                        self.etp_send_to_neigh(etp, nr)
                    ##

    def collision_check(self, gwnip, neigh, R):
        """ Checks if we are colliding with the network of `neigh'.

            It returns True if we are colliding and we are going to rehook.
            !NOTE! the set R will be modified: all the colliding routes will
            be removed.
        """
        
        logging.debug("Etp: collision check: my netid %d and neighbour's netid %d", self.ntkd.neighbour.netid, neigh.netid)

        if neigh.netid == -1: raise Exception('ETP received from a node with netid = -1 (not completely kooked).')

        while self.ntkd.neighbour.netid == -1:
            # I'm not ready to interact.
            time.sleep(0.001)
            micro_block()

        previous_netid = self.ntkd.neighbour.netid

        if neigh.netid == previous_netid:
            return (False, R) # all ok

        # uhm... we are in different networks
        logging.info('Detected Network Collision')

        # ## Calculate the size of the two nets
        # def add(a,b):return a+b
        # mynetsz = reduce(add, self.maproute.node_nb)
        # ngnetsz = reduce(add, map(len, R))

        # TODO At the moment, we dictate that the net with smaller netid is going to give up.
        # Improvement: Find a secure way to detect if we are in one of these 3 cases:
        #  1. The other net surely will give up. So we are not going to.
        #  2. The other net surely won't give up. So we are going to.
        #  3. The other net surely will choose to use the 'lesser netid' method. So we are going to do the same.
        if self.ntkd.neighbour.netid > neigh.netid:
                # we don't care if we are colliding or not. We can simply
                # ignore colliding routes, the rest will be done by the other
                # net.

                ### Remove colliding routes from R
                R = [ [ (dst, rem, hops) 
                            for dst, rem, hops in R[lvl]
                                if self.maproute.node_get(lvl, dst).is_empty() ]
                        for lvl in xrange(self.maproute.levels) ]
                ###
                
                logging.debug("Etp: collision check: just remove colliding routes from R")
                return (False, R)
        ##

        # We are the smaller net.

        ## Check if we are colliding with another (g)node of the neighbour
        ## net
        logging.debug('Etp: we are the smaller net, check if we are colliding with another gnode')

        level = self.maproute.nip_cmp(self.maproute.me, gwnip) + 1
        if level < self.maproute.levels:
                for dst, rem, hops in R[level]:
                        if dst == self.maproute.me[level]:
                                # we are colliding! LET'S REHOOK
                                logging.debug('Etp: let\'s rehook now.')
                                return (True, R)
        ## 

        ## Remove colliding routes directly from our map
        for lvl in xrange(self.maproute.levels):
                for dst, rem, hops in R[lvl]:
                        # The node I know as (lvl, dst) is invalid; it will eventually rehook.
                        self.maproute.route_reset(lvl, dst)
        ##

        #From now on, we are in the new net
        logging.info('From now on, we are in the new net, our network id: %s' % neigh.netid)
        self.ntkd.neighbour.change_netid(neigh.netid)
        logging.log(logging.ULTRADEBUG, 'etp_exec: collision_check: warn neighbours of' + \
                ' my change netid from ' + str(previous_netid) + ' to ' + str(self.ntkd.neighbour.netid))
        my_ip = self.maproute.nip_to_ip(self.maproute.me)
        self.neigh.call_ip_netid_change_broadcast_udp(my_ip, previous_netid, my_ip, self.ntkd.neighbour.netid)
        logging.log(logging.ULTRADEBUG, 'etp_exec: collision_check: called ip_netid_change on broadcast.')

        return (False, R)
