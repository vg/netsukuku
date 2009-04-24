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

from ntk.lib.micro import microfunc
from ntk.lib.event import Event
from ntk.core.route import NullRem, DeadRem

def is_listlist_empty(l):
        """
            Returns true if l=[[],[], ...]
            l is a list of lists.
        """
        return not any(l)

class Etp:
    """Extended Tracer Packet"""

    def __init__(self, radar, maproute):

        self.neigh   =radar.neigh
        self.netid   =radar.netid
        self.maproute=maproute

        self.neigh.events.listen('NEIGH_NEW', self.etp_new_changed)
        self.neigh.events.listen('NEIGH_REM_CHGED', self.etp_new_changed)
        self.neigh.events.listen('NEIGH_DELETED', self.etp_new_dead)

        self.events = Event(['ETP_EXECUTED', 'NET_COLLISION'])

        self.remotable_funcs = [self.etp_exec]

    @microfunc(True)
    def etp_new_dead(self, neigh):
        """Builds and sends a new ETP for the worsened link case."""

        ## Create R
        def gw_is_neigh((dst, gw, rem)):
                return gw == neigh.id
        R=self.maproute.bestroutes_get(gw_is_neigh)
        ##

        ## Update the map
        self.maproute.routeneigh_del(neigh)
        ##

        if is_listlist_empty(R):
                # R is empty, that is we don't have routes passing by `gw'.
                # Therefore, nothing to update, nothing to do.
                return
        
        ## Create R2
        def rem_or_none(r):
                if r is not None:
                        return r.rem
                return DeadRem()

        R2 = [ 
              [ (dst, rem_or_none(self.maproute.node_get(lvl, dst).best_route()))
                    for (dst,gw,rem) in R[lvl]
              ] for lvl in xrange(self.maproute.levels)
             ]
        ##

        ## Forward the ETP to the neighbours
        flag_of_interest=1
        TP = [[self.maproute.me[0], NullRem()]]    # Tracer Packet included in
        block_lvl = 0                           # the first block of the ETP
        etp = (R2, [[block_lvl, TP]], flag_of_interest)
        self.etp_forward(etp, [neigh.id])
        ##

    @microfunc(True)
    def etp_new_changed(self, neigh, oldrem=None):
        """Builds and sends a new ETP for the changed link case

        If oldrem=None, the node `neigh' is considered new."""

        ## Update the map
        if oldrem is None:
            self.maproute.routeneigh_add(neigh)
        else:
            self.maproute.routeneigh_rem(neigh)
        ##

        ## Create R
        def gw_isnot_neigh((dst, gw, rem)):
                return gw != neigh.id
        R = self.maproute.bestroutes_get(gw_isnot_neigh)
        if is_listlist_empty(R):
                # R is empty: no need to proceed
                return

        def takeoff_gw((dst, gw, rem)):
                return (dst, rem)
        def takeoff_gw_lvl(L):
                return map(takeoff_gw, L)
        R=map(takeoff_gw_lvl, R)
        ##

        ## Send the ETP to `neigh'
        flag_of_interest=1
        TP = [[self.maproute.me[0], NullRem()]]
        etp = (R, [[0, TP]], flag_of_interest)
        neigh.ntkd.etp.etp_exec(self.maproute.me, *etp)
        ##

    @microfunc()
    def etp_exec(self, sender_nip, R, TPL, flag_of_interest):
        """Executes a received ETP
        
        sender_nip: sender ntk ip (see map.py)
        R  : the set of routes of the ETP
        TPL: the tracer packet of the path covered until now by this ETP.
             This TP may have covered different levels. In general, TPL 
             is a list of blocks. Each block is a (lvl, TP) pair, where lvl is
             the level of the block and TP is the tracer packet composed
             during the transit in the level `lvl'.
             TP is a list of (hop, rem) pairs.
        flag_of_interest: a boolean
        """

        gwnip   = sender_nip
        neigh   = self.neigh.ip_to_neigh(self.maproute.nip_to_ip(gwnip))
        gw      = neigh.id
        gwrem   = neigh.rem

        ## Collision check
        colliding, R = self.collision_check(gwnip, neigh, R)
        if colliding:
                # collision detected. rehook.
                self.events.send('NET_COLLISION', 
                                 ([nr for nr in self.neigh.neigh_list()
                                                if nr.netid == neigh.netid],)
                                )
                return # drop the packet
        ##

        ## Group rule
        level = self.maproute.nip_cmp(self.maproute.me, gwnip)
        for block in TPL:
                lvl = block[0] # the level of the block
                if lvl < level:
                        block[0] = level                     
                        blockrem = sum([rem for hop, rem in block[1]], NullRem())
                        block[1] = [[gwnip[level], blockrem]]
                        R[lvl] = []
        
        
        ### Collapse blocks of the same level
        #Note: we're assuming the two blocks with the same level are one after
        #      another.
        TPL2=[TPL[0]]
        for block in TPL[1:]:
                if block[0] == TPL2[-1][0]:
                        TPL2[-1][1]+=block[1]
                else:
                        TPL2.append(block)
        TPL=TPL2
        ###
        
        ### Remove dups
        def remove_contiguos_dups_in_TP(L):
                L2=[]
                prec=[None, NullRem()]
                for x in L:
                        if x[0] != prec[0]:
                                prec=x
                                L2.append(x)
                        else:
                                prec[1]+=x[1]
                return L2

        for block in TPL:
                block[1]=remove_contiguos_dups_in_TP(block[1])
        ###

        ##

        ## ATP rule
        for block in TPL:
                if self.maproute.me[block[0]] in block[1]:
                        return    # drop the pkt
        ##

        ## The rem of the first block is useless.
        TPL[0][1][0][1] = NullRem()
        ##

        old_node_nb = self.maproute.node_nb[:]

        ## Update the map from the TPL
        tprem=gwrem
        TPL_is_interesting = False
        for block in reversed(TPL):
                lvl=block[0]
                for dst, rem in reversed(block[1]):
                        if self.maproute.route_change(lvl, dst, gw, tprem):
                                TPL_is_interesting = True
                        tprem+=rem # TODO: sometimes rem is an integer
        ##

        ## Update the map from R
        for lvl in xrange(self.maproute.levels):
                for dst, rem in R[lvl]:
                        if not self.maproute.route_rem(lvl, dst, gw, rem+tprem):
                                self.maproute.route_change(lvl, dst, gw, rem+tprem)
        ##

        ## S
        S = [ [ (dst, r.rem)
                for dst, rem in R[lvl]
                    for r in [self.maproute.node_get(lvl, dst).best_route()]
                        if r.gw != gw
              ] for lvl in xrange(self.maproute.levels) ]
       
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
        #               neigh.ntkd.etp.etp_exec(self.maproute.me, *etp)
        ##

        ## R2 
        R2 = [ [ (dst, rem)
                for dst, rem in R[lvl]
                    if dst not in [d for d, r in S[lvl]]
              ] for lvl in xrange(self.maproute.levels) ]
        ##

        ## Continue to forward the ETP if it is interesting

        if not is_listlist_empty(R2) or TPL_is_interesting:
                if TPL[-1][0] != 0: 
                        # The last block isn't of level 0. Let's add a new block
                        TP = [[self.maproute.me[0], gwrem]] 
                        TPL.append([0, TP])
                else:
                        # The last block is of level 0. We can append our ID
                        TPL[-1][1].append([self.maproute.me[0], gwrem])


                etp = (R2, TPL, flag_of_interest)
                self.etp_forward(etp, [neigh.id])
        ##

        self.events.send('ETP_EXECUTED', (old_node_nb, self.maproute.node_nb[:]))

    def etp_forward(self, etp, exclude):
        """Forwards the `etp' to all our neighbours,
           excluding those contained in `exclude'
           
           `Exclude' is a list of "Neighbour.id"s"""

        for nr in self.neigh.neigh_list():
                if nr.id not in exclude:
                        nr.ntkd.etp.etp_exec(self.maproute.me, *etp)
    
    def collision_check(self, gwnip, neigh, R):
        """ Checks if we are colliding with the network of `neigh'.
        
            It returns True if we are colliding and we are going to rehook.
            !NOTE! the set R will be modified: all the colliding routes will
            be removed.
        """
        
        if neigh.netid == self.netid                              \
            or self.netid == -1:
                self.netid = neigh.netid
                return (False, R) # all ok

        # uhm... we are in different networks

        ## Calculate the size of the two nets
        def add(a,b):return a+b
        mynetsz = reduce(add, self.maproute.node_nb)
        ngnetsz = reduce(add, map(len, R))

        if mynetsz > ngnetsz or                                         \
                (mynetsz == ngnetsz and self.netid > neigh.netid):
                # we don't care if we are colliding or not. We can simply
                # ignore colliding routes, the rest will be done by the other
                # net.

                ### Remove colliding routes from R
                R = [ [ (dst, rem) 
                            for dst, rem in R[lvl]
                                if self.maproute.node_get(lvl, dst).is_empty() ]
                        for lvl in xrange(self.maproute.levels) ]
                ###
                return (False, R)
        ##

        # We are the smaller net.

        ## Check if we are colliding with another (g)node of the neighbour
        ## net
        level = self.maproute.nip_cmp(self.maproute.me, gwnip) + 1
        if level < self.maproute.levels:
                for dst, rem in R[level]:
                        if dst == self.maproute.me[level]:
                                # we are colliding! LET'S REHOOK
                                return (True, R)
        ## 

        ## Remove colliding routes directly from our map
        for lvl in xrange(self.maproute.levels):
                for dst, rem in R[lvl]:
                        self.maproute.node_get(lvl, dst).route_reset()
        ##

        return (False, R)

