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

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime

from ntk.lib.log import logger as logging
from random import choice, randint


from ntk.lib.micro import microfunc, Channel
from ntk.lib.event import Event

from ntk.network.inet import ip_to_str, valid_ids


class Hook(object):

    def __init__(self, ntkd, radar, maproute, etp, coordnode, nics): 
        self.ntkd = ntkd
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        self.etp = etp
        self.coordnode= coordnode
        self.nics = nics

        self.chan_replies = Channel()

        self.events = Event(['HOOKED'])

        self.etp.events.listen('ETP_EXECUTED', self.communicating_vessels)
        self.etp.events.listen('NET_COLLISION', self.hook)

        self.remotable_funcs = [self.communicating_vessels,
                                self.highest_free_nodes,
                                self.highest_free_nodes_udp,
                                self.reply_highest_free_nodes_udp]

    @microfunc()
    def communicating_vessels(self, old_node_nb=None, cur_node_nb=None):
        '''Note: old_node_nb and cur_node_nb are used only by the ETP_EXECUTED event'''

        logging.debug('Coomunicating vessels microfunc started')
        current_nr_list = self.neigh.neigh_list()
        
        if old_node_nb != None and self.gnodes_split(old_node_nb, cur_node_nb):
                # The gnode has splitted and we have rehooked.
                # Nothing to do anymore.
                return

        if cur_node_nb and old_node_nb and cur_node_nb[0] == old_node_nb[0]:
                # No new or dead node in level 0
                logging.debug('Coomunicating vessels: No new or dead node in level 0')
                return

        candidates=[]   # List of (neigh, fnb) elements. neigh is a
                        # Neigh instance; fnb is the number of free
                        # of the level 0 of neigh
        inv_candidates=[]
        def cand_cmp((a1, a2), (b1, b2)):
            return cmp(a2, b2)

        for nr in current_nr_list:
                nrnip=self.maproute.ip_to_nip(nr.ip)
                if self.maproute.nip_cmp(self.maproute.me, nrnip) <= 0:
                        # we're interested in external neighbours
                        continue
                fnb=nr.ntkd.maproute.free_nodes_nb(0)
                logging.log(logging.ULTRADEBUG, str(nr) + ' has replied its gnode has ' + str(fnb) + ' free nodes.')
                if fnb+1 < self.maproute.free_nodes_nb(0):
                        inv_candidates.append((nr, fnb))
                elif self.maproute.free_nodes_nb(0)+1 < fnb:
                        candidates.append((nr, fnb))

        if inv_candidates:
                inv_candidates.sort(cmp=cand_cmp)
                # tell our neighbour, which is bigger than us, to launch 
                # its communicating vessels system
                inv_candidates[0][0].ntkd.hook.communicating_vessels()

        if candidates:
                candidates.sort(cmp=cand_cmp, reverse=1)
                # We've found some neighbour gnodes smaller than us. 
                # Let's rehook
                self.hook([nr for (nr, fnb) in candidates], [], True,
                                candidates[0][1])

                ##TODO: Maybe self.hook() should be done BEFORE forwarding the
                #       ETP that has generated the ETP_EXECUTED event.

        logging.debug('Coomunicating vessels microfunc end')

    @microfunc()
    def hook(self, passed_neigh_list=[], forbidden_neighs=[], condition=False, gfree_new=0):
        """Lets the current node become a hooking node.

        If passed_neigh_list!=[], it tries to hook among the given neighbours
        list, otherwise neigh_list is generated from the Radar.

        forbidden_neighs is a list [(lvl,nip), ...]. All the neighbours nr with a
        NIP nr.nip matching nip up to lvl levels are excluded, that is:
                NIP is excluded <==> nip_cmp(nip, NIP) < lvl
        
        Note: the following is used only by communicating_vessels()
        
        If condition is True, the re-hook takes place only if the following
        is true:
                gfree_new > gfree_old_coord
                gfree_new_coord > gfree_old_coord
        where "gfree" is the number of free nodes of the gnode; "new" refers
        to the gnode where we are going to re-hook, "old" to the current one
        where we belong; the suffix "_coord" means that is calculated by the
        Coordinator node.
        gfree_new is passed to the function as a parameter. We have obtained
        it from our neighbour with the remotable method 
        maproute.free_nodes_nb(0).
        In contrast, we request the other values to the Coordinator.
        """

        # The method 'hook' is called in various situations. How do we
        # detect them?
        # 1. hook called at bootstrap. We are in a private gnode (192.168..).
        #    We get called without any argument.
        # 2. hook called for a gnode splitting. We have some
        #    "forbidden_neighs".
        # 3. hook called for a communicating vessels. We have some
        #    "candidates" in "passed_neigh_list". We have a "condition" to
        #    satisfy too.
        # 4. hook called for a network collision. We have some nodes of the
        #    new network in "passed_neigh_list". But, we don't have any
        #    "condition" to satisfy.
        called_for_bootstrap = 1
        called_for_gnode_splitting = 2
        called_for_communicating_vessels = 3
        called_for_network_collision = 4
        called_for = None
        if forbidden_neighs:
            called_for = called_for_gnode_splitting
        elif not passed_neigh_list:
            called_for = called_for_bootstrap
        elif condition:
            called_for = called_for_communicating_vessels
        else:
            called_for = called_for_network_collision

        logging.info('Hooking procedure started.')
        current_nr_list = self.neigh.neigh_list()
        previous_netid = self.ntkd.neighbour.netid
        if previous_netid != -1:
            logging.info('We previously had got a network id = ' + str(previous_netid))
        logging.info('We haven\'t got any network id, now.')
        self.ntkd.neighbour.netid = -1
        oldnip = self.maproute.me[:]
        oldip = self.maproute.nip_to_ip(oldnip)
        we_are_alone = False
        suitable_neighbours = []

        neigh_list = passed_neigh_list
        if neigh_list == []:
                neigh_list = current_nr_list
        if neigh_list == []:
                we_are_alone = True

        hfn = []
        ## Find all the highest non saturated gnodes

        def is_current_hfn_valid():
            # Should our actual knowledge of the network be considered?
            # TODO Review implementation
            if called_for == called_for_network_collision:
                return False
            if called_for == called_for_communicating_vessels:
                return False
            if called_for == called_for_bootstrap:
                if we_are_alone:
                    return True
                else:
                    return False
            if called_for == called_for_gnode_splitting:
                return False
            raise Exception('Hooking phase does not recognize the situation.')
        if is_current_hfn_valid():
            hfn = [(self.maproute.me, None, self.highest_free_nodes())]
            logging.info('Hook: highest non saturated gnodes that I know: ' + str(hfn))
        else:
            logging.info('Hook: highest non saturated gnodes that I know is irrelevant.')

        def is_neigh_forbidden(nrip):
                for lvl, fnr in forbidden_neighs:
                        if self.maproute.nip_cmp(nrnip, fnr) < lvl:
                                return True
                return False

        for nr in neigh_list:
                nrnip=self.maproute.ip_to_nip(nr.ip)

                # Do we have to avoid hooking with a neighbour that is in our
                # gnode of level 1? Based on the situation:
                # 1. hook called at bootstrap. We are in a private gnode (192.168...)
                #    In this case we must avoid neighbours with private gnode. Anyway
                #    such a neighbour would not be present in self.neigh.neigh_list()
                #    because it won't respond to radar scan
                # 2. hook called for a gnode-splitting. We are the smaller part of
                #    the splitted gnode, we must go in another gnode. But, this is
                #    accomplished with the use of "forbidden_neighs"
                # 3. hook called for a communicating vessels. We want to change our
                #    gnode of level 1. But the method communicating_vessels already
                #    has this control, so such a neighbour would not be in
                #    "candidates".
                # 4. hook called for a network collision. In this case we want to
                #    use only neighbours in the new network.
                #    We don't want to exclude a neighbour in the new network which
                #    incidentally has our same previous gnode.
                # So the answer is "not".

                if is_neigh_forbidden(nrnip):
                        # We don't want forbidden neighbours
                        continue

                suitable_neighbours.append(nr)

        for nr in suitable_neighbours:
                nrnip = self.maproute.ip_to_nip(nr.ip)
                nr_hfn = self.call_highest_free_nodes_udp(nr)
                hfn.append((nrnip, nr, nr_hfn))
                logging.info('Hook: highest non saturated gnodes that ' + str(nrnip) + ' knows: ' + str(nr_hfn))
        ##

        ## Find all the hfn elements with the highest level and
        ## remove all the lower ones
        hfn2 = []
        hfn_lvlmax = -1
        for h in hfn:
                if h[2][0] > hfn_lvlmax:
                        hfn_lvlmax = h[2][0]
                        hfn2=[]
                if h[2][0] == hfn_lvlmax:
                        hfn2.append(h)
        hfn = hfn2
        ##

        ## Find the list with the highest number of elements
        lenmax = 0
        for h in hfn:
            if h[2][1] is not None:
                l = len(h[2][1])
                if l > lenmax:
                        lenmax=l
                        H=h
        ##

        if lenmax == 0:
                raise Exception, "Netsukuku is full"

        ## Generate part of our new IP
        newnip = list(H[0])
        neigh_respond = H[1]
        lvl = H[2][0]
        if neigh_respond is None:
            logging.log(logging.ULTRADEBUG, 'Hook: the best is level ' + str(lvl) \
                    + ' known by myself')
        else:
            logging.log(logging.ULTRADEBUG, 'Hook: the best is level ' + str(lvl) \
                    + ' known by ' + str(H[0]) \
                    + ' with netid ' + str(neigh_respond.netid))
        fnl = H[2][1]
        newnip[lvl] = choice(fnl)
        logging.log(logging.ULTRADEBUG, 'Hook: we choose ' + str(newnip[lvl]))
        for l in reversed(xrange(lvl)): newnip[l] = choice(valid_ids(l, newnip))
        logging.log(logging.ULTRADEBUG, 'Hook: our option is ' + str(newnip))

        # If we are alone, let's generate our netid
        if we_are_alone:
            self.ntkd.neighbour.netid = randint(0, 2**32-1)
            logging.info("Generated our network id: %s", self.ntkd.neighbour.netid)
            # and we don't need to contact coordinator node...

        else:
            # Contact the coordinator node.

            # If we are called for bootstrap, we could
            # have neighbours in different networks (netid).
            # And we for sure are not the coordinator node for
            # the gnode we will enter.
            #
            # If we are called for network collision, we will contact
            # neighbours in a different network.
            # And we for sure are not the coordinator node for
            # the gnode we will enter.
            #
            # In these 2 cases we want to reach the coordinator node for
            # the gnode we will enter, passing through the neighbour which
            # we asked for the hfn.
            if called_for == called_for_bootstrap or called_for == called_for_network_collision:
                neighudp = neigh_respond

            # In the other situations, we don't
            # have neighbours in different networks (netid).
            # And we have the right knowledge of the network.
            # So, the p2p module knows the best neighbour to use to reach the
            # coordinator node for the gnode we will enter.
            else:
                neighudp = None

            # TODO We must handle the case of error in contacting the
            #   coordinator node. The coordinator itself may die.

            if lvl > 0:
                # If we are going to create a new gnode, it's useless to pose
                # any condition
                condition=False
                # TODO Do we have to tell to the old Coord that we're leaving?

            if condition:
                # <<I'm going out>>
                logging.log(logging.ULTRADEBUG, 'Hook: going_out, in order to' + \
                     ' join a gnode which has ' + str(gfree_new) + ' free nodes.')
                co = self.coordnode.peer(key=(1, self.maproute.me), use_udp=True, neighudp=neighudp)
                # get gfree_old_coord and check that  gfree_new > gfree_old_coord
                gfree_old_coord = co.going_out(0, self.maproute.me[0], gfree_new)
                if gfree_old_coord is None:
                    # nothing to be done
                    logging.info('Hooking procedure canceled by our Coord. Our' + \
                                 ' network id is back.')
                    self.ntkd.neighbour.netid = previous_netid
                    return

                # <<I'm going in, can I?>>
                logging.log(logging.ULTRADEBUG, 'Hook: going_in with' + \
                           ' gfree_old_coord = ' + str(gfree_old_coord))
                co2 = self.coordnode.peer(key = (lvl+1, newnip), use_udp=True, neighudp=neighudp)
                # ask if we can get in and if gfree_new_coord > gfree_old_coord,
                # and get our new IP
                newnip=co2.going_in(lvl, gfree_old_coord)

                if newnip:
                    # we've been accepted
                    co.going_out_ok(0, self.maproute.me[0])
                else:
                    raise Exception, "Netsukuku is full"

                # TODO do we need to implement a close?
                #co.close()
                #co2.close()

            else:
                # <<I'm going in, can I?>>
                logging.log(logging.ULTRADEBUG, 'Hook: going_in without' + \
                            ' condition.')
                co2 = self.coordnode.peer(key = (lvl+1, newnip), use_udp=True, neighudp=neighudp)
                # ask if we can get in, get our new IP
                logging.info('Hook: contacting coordinator node...')
                newnip=co2.going_in(lvl)
                logging.info('Hook: contacted coordinator node, assigned nip' + \
                             ' = ' + str(newnip))
                if newnip is None:
                    raise Exception, "Netsukuku is full"
                # TODO do we need to implement a close?
                #co2.close()
        ##

        logging.log(logging.ULTRADEBUG, 'Hook: completing hook...')

        ## complete the hook
        self.radar.do_reply = False

        # close the ntkd sessions
        self.neigh.reset_ntk_clients()

        # change the IPs of the NICs
        newnip_ip = self.maproute.nip_to_ip(newnip)
        self.nics.activate(ip_to_str(newnip_ip))

        # reset the map
        self.maproute.map_reset()
        logging.info('MapRoute cleaned because NICs have been flushed')
        self.maproute.me_change(newnip[:])
        self.coordnode.mapcache.me_change(newnip[:], silent=True)
        for l in reversed(xrange(lvl)): self.maproute.level_reset(l)

        # Restore the neighbours in the map and send the ETP
        self.neigh.readvertise()

        # warn our neighbours
        if previous_netid == -1 or we_are_alone:
            logging.log(logging.ULTRADEBUG, 'Hook.hook warn neighbours' + \
                    ' skipped')
        else:
            logging.log(logging.ULTRADEBUG, 'Hook.hook warn neighbours of' + \
                    ' my change from %s to %s.' \
                    % (ip_to_str(oldip), ip_to_str(newnip_ip)))
            for nr in current_nr_list:
                logging.log(logging.ULTRADEBUG, 'Hook: calling ip_netid_change' + \
                            ' of my neighbour %s.' % ip_to_str(nr.ip)) 
                self.neigh.call_ip_netid_change_udp(nr, oldip, previous_netid, newnip_ip, self.ntkd.neighbour.netid)
                logging.log(logging.ULTRADEBUG, 'Hook: %s ack.' \
                        % ip_to_str(nr.ip)) 

        # now that our neighbours have been warned, we can reply to their
        # radar scans
        self.radar.do_reply = True
        logging.log(logging.ULTRADEBUG, 'Hook: done. Now we should be' + \
                    ' able to use TCP')

        # we've done our part
        logging.info('Hooking procedure completed.')
        if self.ntkd.neighbour.netid == -1:
            logging.info('We haven\'t got any network id yet.')
        else:
            self.etp.events.send('COMPLETE_HOOK', ())
        self.events.send('HOOKED', (oldip, newnip[:]))
        ##


    def highest_free_nodes(self):
        """Returns (lvl, fnl), where fnl is a list of free node IDs of
           level `lvl'."""
        for lvl in reversed(xrange(self.maproute.levels)):
                fnl = self.maproute.free_nodes_list(lvl)
                if fnl:
                        return (lvl, fnl)
        return (-1, None)

    def call_highest_free_nodes_udp(self, neigh):
        """Use BcastClient to call highest_free_nodes"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'hook.highest_free_nodes_udp')

    def highest_free_nodes_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid):
        """Returns the result of highest_free_nodes to remote caller.
           caller_id is the random value generated by the caller for this call.
            It is replied back to the LAN for the caller to recognize a reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated to it.
           """
        if self.maproute.me == callee_nip and self.neigh.netid == callee_netid:
            ret = self.highest_free_nodes()
            rpc.UDP_send_reply(_rpc_caller, caller_id, 'hook.reply_highest_free_nodes_udp', ret)

    def reply_highest_free_nodes_udp(self, _rpc_caller, caller_id, ret):
        """Receives reply from highest_free_nodes_udp."""
        rpc.UDP_got_reply(_rpc_caller, caller_id, ret)

    def gnodes_split(self, old_node_nb, cur_node_nb):
        """Handles the case of gnode splitting

           Returns True if we have to rehook"""

        gnodesplitted = 0

        # Note: at level self.maproute.levels-1 is meaningless to talk about
        # gnode splitting, in fact, is the whole network that has been splitted
        levels = self.maproute.levels-1

        for lvl in reversed(xrange(levels)):
                diff = old_node_nb[lvl] - cur_node_nb[lvl]
                if diff > 0 and diff >= cur_node_nb[lvl]:
                        level = lvl+1
                        gnodesplitted=1
                        break # We stop at the highest level
        if not gnodesplitted:
                return False

        ##TODO: gnode splitting can be handled in a different way:
        #        if the ETP has been generated by 'NEIGH_DELETED'
        #        and we are in the smaller part, rehook  and _THEN_ forward the
        #        ETP.
        #        Is it better? Maybe not.

        # ok, our gnode of level `level' has become broken, and we are in the
        # smallest part of the two. Let's rehook. However, be sure to to rehook
        # in a place different from the splitted gnode
        forbidden_neighs = [(level, [0]*level+self.maproute.me[level:])]
        self.hook(neigh_list=[], forbidden_neighs=forbidden_neighs)
        return True
