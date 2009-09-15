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
from random import choice, randint

from ntk.lib.micro import microfunc
from ntk.lib.event import Event

from ntk.network.inet import ip_to_str

class Hook(object):

    def __init__(self, radar, maproute, etp, coordnode, nics): 
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        self.etp = etp
        self.coordnode= coordnode
        self.nics = nics

        self.events = Event(['HOOKED'])

        etp.events.listen('ETP_EXECUTED', self.communicating_vessels)
        etp.events.listen('NET_COLLISION', self.hook)

        self.remotable_funcs = [self.communicating_vessels,
                                self.highest_free_nodes]

    @microfunc()
    def communicating_vessels(self, old_node_nb=None, cur_node_nb=None):
        '''Note: old_node_nb and cur_node_nb are used only by the ETP_EXECUTED event'''

        if old_node_nb != None and self.gnodes_split(old_node_nb, cur_node_nb):
                # The gnode has splitted and we have rehooked. 
                # Nothing to do anymore.
                return 

        if cur_node_nb and old_node_nb and cur_node_nb[0] == old_node_nb[0]:
                # No new or dead node in level 0
                return

        candidates=[]   # List of (neigh, fnb) elements. neigh is a
                        # Neigh instance; fnb is the number of free
                        # of the level 0 of neigh
        inv_candidates=[]
        def cand_cmp((a1, a2), (b1, b2)):
                return cmp(a2, b2)

        for nr in self.neigh.neigh_list():
                nrnip=self.maproute.ip_to_nip(nr.ip)
                if self.maproute.nip_cmp(self.maproute.me, nrnip) <= 0:
                        # we're interested in external neighbours
                        continue
                fnb=nr.ntkd.maproute.free_nodes_nb(0)
                if fnb+1 < self.maproute.free_nodes_nb(0):
                        candidates.append((nr, fnb))
                elif self.maproute.free_nodes_nb(0)+1 < fnb:
                        inv_candidates.append((nr, fnb))

        if inv_candidates:
                inv_candidates.sort(cmp=cand_cmp, reverse=1)
                # tell our neighbour, which is bigger than us, to launch 
                # its communicating vessels system
                inv_candidates[0][0].ntkd.hook.communicating_vessels()

        if candidates:
                candidates.sort(cmp=cand_cmp)
                # We've found some neighbour gnodes smaller than us. 
                # Let's rehook
                self.hook([nr for (nr, fnb) in candidates], [], True,
                                candidates[0][1])

                ##TODO: Maybe self.hook() should be done BEFORE forwarding the
                #       ETP that has generated the ETP_EXECUTED event.

    @microfunc()
    def hook(self, neigh_list=[], forbidden_neighs=[], condition=False, gnumb=0):
        """Lets the current node become a hooking node.

        I neigh_list!=[], it tries to hook among the given neighbours
        list, otherwise neigh_list is generated from the Radar.

        forbidden_neighs is a list [(lvl,nip), ...]. All the neighbours nr with a
        NIP nr.nip matching nip up to lvl levels are excluded, that is:
                NIP is excluded <==> nip_cmp(nip, NIP) < lvl
        
        If condition is True, the re-hook take place only if the following
        is true:
                gnumb < |G|
                |G'| < |G|
        where |G'| and `gnumb' is the number of nodes of the gnode where 
        we are going to re-hook, and |G| is the number of nodes of our
        gnodes. |G'| and |G| are calculated using the coordinator
        nodes. Note: this is used only by communicating_vessels()
        """
        
        oldnip=self.maproute.me[:]
        we_are_alone=False

        ## Find all the highest non saturated gnodes
        hfn = [(self.maproute.me, self.highest_free_nodes())]
        
        if neigh_list == []:
                neigh_list = self.neigh.neigh_list()
        if neigh_list == []:
                we_are_alone = True
        
        def is_neigh_forbidden(nrip):
                for lvl, fnr in forbidden_neighs:
                        if self.maproute.nip_cmp(nrnip, fnr) < lvl:
                                return True
                return False

        for nr in neigh_list:
                nrnip=self.maproute.ip_to_nip(nr.ip)

                if self.maproute.nip_cmp(self.maproute.me, nrnip) <= 0:
                        # we're interested in external neighbours 
                        continue
                if is_neigh_forbidden(nrnip):
                        # We don't want forbidden neighbours
                        continue
                hfn.append((nrnip, nr.ntkd.hook.highest_free_nodes()))
        ##

        ## Find all the hfn elements with the highest level and 
        ## remove all the lower ones
        hfn2=[]
        hfn_lvlmax = -1
        for h in hfn:
                if h[1][0] > hfn_lvlmax:
                        hfn_lvlmax = h[1][0]
                        hfn2=[]
                if h[1][0] == hfn_lvlmax:
                        hfn2.append(h)
        hfn=hfn2
        ##

        ## Find the list with the highest number of elements
        lenmax = 0
        for h in hfn:
                l=len(h[1][1])
                if l > lenmax:
                        lenmax=l
                        H=h
        ##

        if lenmax == 0:
                raise Exception, "Netsukuku is full"

        ## Generate part of our new IP
        newnip = list(H[0])
        lvl = H[1][0]
        fnl = H[1][1]
        newnip[lvl] = choice(fnl)
        for l in reversed(xrange(lvl)): newnip[l]=randint(0, self.maproute.gsize-1)

        # If we are alone, let's generate our netid
        if we_are_alone: self.radar.netid = randint(0, 2**32-1)
        ##


        if lvl < self.maproute.levels-1:
                # We are creating a new gnode which is not in the latest
                # level. 
                # Contact the coordinator nodes 
                
                if lvl > 0:
                        # If we are going to create a new gnode, it's useless to pose
                        # any condition
                        condition=False

                if condition:
                        # <<I'm going out>>
                        co = self.coordnode.peer(key = (1, self.maproute.me))
                        # get |G| and check that  gnumb < |G|
                        Gnumb = co.going_out(0, self.maproute.me[0], gnumb)
                        if Gnumb == None:
                                # nothing to be done
                                return

                        # <<I'm going in, can I?>>
                        co2 = self.coordnode.peer(key = (lvl+1, newnip))
                        # ask if we can get in and if |G'| < |G|, and get our new IP
                        newnip=co2.going_in(lvl, Gnumb)

                        if newnip:
                                # we've been accepted
                                co.going_out_ok(0, self.maproute.me[0])
                        else:
                                raise Exception, "Netsukuku is full"

                        co.close()
                        co2.close()
                elif not we_are_alone:
                        # <<I'm going in, can I?>>
                        co2 = self.coordnode.peer(key = (lvl+1, newnip))
                        # ask if we can get in, get our new IP
                        newnip=co2.going_in(lvl)
                        if newnip == None:
                                raise Exception, "Netsukuku is full"
                        co2.close()
        ##

        
        ## complete the hook
        self.radar.do_reply = False

        # close the ntkd sessions
        for nr in self.neigh.neigh_list():
                if nr.ntkd.connected:
                        nr.ntkd.close()

        # change the IPs of the NICs
        newnip_ip = self.maproute.nip_to_ip(newnip)
        self.nics.activate(ip_to_str(newnip_ip))

        # reset the map
        self.maproute.me_change(newnip[:])
        for l in reversed(xrange(lvl)): self.maproute.level_reset(l)

        # Restore the neighbours in the map and send the ETP
        self.neigh.readvertise()
        
        # warn our neighbours
        oldip = self.maproute.nip_to_ip(oldnip)
        for nr in self.neigh.neigh_list():
                logging.debug("Hook: calling ip_change of my neighbour %s" % ip_to_str(nr.ip)) 
                nr.ntkd.neighbour.ip_change(oldip, newnip_ip)
        
        self.radar.do_reply = True

        # we've done our part
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
        forbidden_neighs = (level, [0]*level+self.maproute.me[level:])
        self.hook(neigh_list=[], forbidden_neighs=forbidden_neighs)
        return True
