##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# (c) Copyright 2010 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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
# Implementation of the P2P Over Ntk RFC. See {-P2PNtk-}
#

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime

from ntk.lib.log import ExpectableException
from ntk.config import settings
from ntk.core.map import Map
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.log import get_stackframes
from ntk.lib.micro import microfunc, start_tracking, stop_tracking, micro_current, micro_kill
from ntk.lib.rencode import serializable
from ntk.lib.rpc import FakeRmt, RPCDispatcher, CallerInfo, RPCError
from ntk.core.status import ZombieException 


class P2PError(Exception):
    '''Generic P2P Error'''

class P2PHookingError(ExpectableException):
    '''To signal we are still hooking'''

class ParticipantNode(object):
    def __init__(self, the_map, lvl, id, participant=False, its_me=False):
        self.lvl = lvl
        self.id = id
        self.its_me = its_me
        self.participant = participant

    def is_free(self):
        '''Override the is_free() method of DataClass (see map.py)'''
        # DON'T DO THIS:   if self.its_me: return False
        # Myself, I might be not participant!
        return not self.participant

    def _pack(self):
        # lvl and id are not used (as for now) at the time of 
        # de-serialization. Nor it is the_map.
        # So use the value that will produce the 
        # smaller output with rencode.dumps.
        # TODO test what this value is... perhaps None is better than 0 ?
        return (0, 0, 0, self.participant)

    def __repr__(self):
        return '<%s: %s>' % (self.__class__.__name__, self.participant)

serializable.register(ParticipantNode)

class MapP2P(Map):
    """Map of the participant nodes"""

    def __init__(self, levels, gsize, me, pid):
        """levels, gsize, me: the same of Map

        pid: P2P id of the service associated to this map
        """

        Map.__init__(self, levels, gsize, ParticipantNode, me)

        self.pid = pid

    def participant_node_add(self, lvl, id):
        # It is called:
        #  * by participate, when I begin to participate to this service.
        #  * by participant_add, when another node lets me know it
        #       participates.

        # We have to check (see comment to method Map.node_add)
        if self.node_get(lvl, id).is_free():
                self.node_get(lvl, id).participant = True
                self.node_add(lvl, id)
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P (PID ' + str(self.pid) + ') updated after participant_node_add: ' + 
                    str(self.repr_me()))

    def participant_node_del(self, lvl, id):
        # It is called:
        #  * by sit_out, when I'm going out from this service.
        #  * by participant_del, when another node lets me know it
        #       has gone from this service.
        #  * by node_dead, when another node dies (no routes to it).

        # We don't have to check (see comment to method Map.node_del)
        self.node_del(lvl, id)
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P (PID ' + str(self.pid) + ') updated after participant_node_del: ' + 
                    str(self.repr_me()))
        
    def change_nip(self, old_me, new_me):
        '''Changes self.me

        :param old_me: my old nip (not used in MapP2P)
        :param new_me: new nip
        '''
        self.me_change(new_me)
        # This map has to forget all it knows about the network, because
        # we might have hooked on another network.
        # TODO: should this logic go up at the Map class? Or we could
        #       have particular maps where this logic is not what we want?
        self.map_reset()
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P (PID ' + str(self.pid) + ') updated after '
                               'me_changed: ' + str(self.repr_me()))

    @microfunc(True)
    def node_dead(self, lvl, id):
        # A node died. So it is no more participating.
        self.participant_node_del(lvl, id)

    def participate(self):
        """Set self.me to be a participant node."""

        for l in xrange(self.levels):
            self.participant_node_add(l, self.me[l])

    def sit_out(self):
        """Set self.me to be a participant node."""

        for l in xrange(self.levels):
            self.participant_node_del(l, self.me[l])
            
    def map_data_pack(self):
        """Prepares a packed_mapp2p to be passed to mapp2p.map_data_merge
        in another host."""
        def fmake_participant(node):
            node.participant = True
        return Map.map_data_pack(self, fmake_participant)

    def map_data_merge(self, (nip, plist, nblist)):
        """Copies a mapp2p from another nip's point of view."""
        logging.log(logging.ULTRADEBUG, 'Merging a MapP2P (PID ' + str(self.pid) + '): '
                    'start: ' + self.repr_me())
        # Was I participant?
        me_was = [False] * self.levels
        for lvl in xrange(self.levels):
            me_was[lvl] = self.node_get(lvl, self.me[lvl]).participant
        # This map has to forget all it knows about the network, because
        # we might have hooked on another network.
        # TODO: should this logic go up at the Map class? Or we could
        #       have particular maps where this logic is not what we want?
        self.map_reset()
        logging.log(logging.ULTRADEBUG, 'Merging a MapP2P (PID ' + str(self.pid) + '): '
                    'after Map.map_reset: ' + self.repr_me())
        # Merge as usual...
        lvl=self.nip_cmp(nip, self.me)
        logging.log(logging.ULTRADEBUG, 'Merging a mapp2p at level ' + 
                    str(lvl))
        logging.log(logging.ULTRADEBUG, get_stackframes(back=1))
        Map.map_data_merge(self, (nip, plist, nblist))
        logging.log(logging.ULTRADEBUG, 'Merging a MapP2P (PID ' + str(self.pid) + '): '
                    'after Map.map_data_merge: ' + self.repr_me())
        # ... ripristine myself.
        for lvl in xrange(self.levels):
            if me_was[lvl]:
                self.participant_node_add(lvl, self.me[lvl])
        logging.log(logging.ULTRADEBUG, 'Merging a MapP2P (PID ' + str(self.pid) + '): '
                    'after ripristine myself: ' + self.repr_me())

    def repr_me(self, func_repr_node=None):
        def repr_node_mapp2p(node):
            if node.participant: return 'X'
            return ' '
        if func_repr_node is None: func_repr_node = repr_node_mapp2p
        return Map.repr_me(self, func_repr_node)

class P2P(RPCDispatcher):
    """ This is the class that must be inherited to create a Strict P2P module
        service. A strict service is a service where all the hosts connected 
        to Netsukuku are participant, so the MapP2P is not used here. """

    def __init__(self, ntkd_status, radar, maproute, pid):
        """radar, maproute: the instances of the relative modules

           pid: P2P id of the service associated to this map
        """

        RPCDispatcher.__init__(self, root_instance=self)

        self.ntkd_status = ntkd_status
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        self.pid = pid

        self.remotable_funcs = [self.msg_send,
                                self.msg_send_udp,
                                self.find_nearest_send]

        # From the moment I change my NIP up to the moment I have hooked,
        # we don't want to exploit P2P mechanism. That is msg_send,
        # find_nearest_send, and the like.
        # There are exceptions. E.g. the module Hook will make
        # a request to Coordinator service.
        # Such a request will use the argument neigh of method peer.
        #     self.coordnode.peer(key = (lvl+1, newnip), neigh=neigh)
        # So it will use msg_send_udp. It must be permitted.
        self.wait_p2p_hook = True
        self.maproute.events.listen('ME_CHANGED', self.enter_wait_p2p_hook)

    def enter_wait_p2p_hook(self, *args):
        self.wait_p2p_hook = True

    def exit_wait_p2p_hook(self):
        self.wait_p2p_hook = False

    def is_participant(self, lvl, idn):
        """Returns True iff the node lvl,idn is participating
        to the service. For a "strict P2P" it means iff the node
        exists in maproute.
        An inheriting class could override the function.
        """
        return not self.maproute.node_get(lvl, idn).is_free()

    def repr_participants(self):
        """Returns a representation of the map of participants.
        An inheriting class could override the function.
        """
        return self.maproute.repr_me()

    def h(self, key):
        """This is the function h:KEY-->hIP.

        You should override it with your own mapping function.
        """
        return key

    def H(self, hIP):
        """This is the function that maps each IP to an existent hash node IP
           If there are no participants, an exception is raised"""
        mp = self.maproute
        logging.log(logging.ULTRADEBUG, 'H: H(' + str(hIP) + ')')
        H_hIP = [None] * mp.levels
        for l in reversed(xrange(mp.levels)):
            for id in xrange(mp.gsize):
                for sign in [-1,1]:
                    hid=(hIP[l] + id * sign) % mp.gsize
                    if self.is_participant(l, hid):
                        H_hIP[l] = hid
                        break
                if H_hIP[l] is not None:
                    break
            if H_hIP[l] is None:
                logging.warning('P2P: H returns None. Map ' + 
                                str(self.repr_participants()))
                raise Exception, 'P2P: H returns None. Map not in sync?'

            if H_hIP[l] != mp.me[l]:
                # we can stop here
                break

        logging.log(logging.ULTRADEBUG, 'H: H(' + str(hIP) + ') = ' + 
                    str(H_hIP))
        return H_hIP

    def neigh_get(self, hip):
        """Returns the Neigh instance of the neighbour we must use to reach
           the hash node.

        `hip' is the IP of the hash node.
        If nothing is found, None is returned
        """

        lvl = self.maproute.nip_cmp(hip, self.maproute.me)
        br = self.maproute.node_get(lvl, hip[lvl]).best_route()
        # TODO choose the best route that does not contain the
        #      gnode from which we received the message, or better
        #      any of the gnodes already touched by the message.

        if not br:
            return None
        return br.gw

    def find_nearest_exec(self, nip, number_of_nodes, lvl, path=0):
        ''' Considering myself as the gnode of level lvl, search
            inside me the nearest number_of_nodes nodes to the
            nip.
            path = 0 means going up and down
            path = 1 means going only up
            path = -1 means going only down
        '''
        # The initial call would be
        #   self.find_nearest_exec(nip, 40, self.maproute.levels)
        # to search for them inside the whole network

        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        sequence = []
        gsize = self.maproute.gsize
        me_as_gnode = self.maproute.me[lvl:]
        center = nip[lvl-1]
        if path == 0: ids = self.find_nearest_up_and_down(center)
        if path == 1: ids = self.find_nearest_up(center)
        if path == -1: ids = self.find_nearest_down(center)
        for i in ids:
            # if gnode me_as_gnode + [i] is partecipant
            if self.is_participant(lvl-1, i):
                # if l > 0
                if lvl-1 > 0:
                    try:
                        # ask to the inner gnode, let's see how many it finds.
                        ask_to_gnode = [i] + me_as_gnode
                        resp = self.find_nearest_send(ask_to_gnode, \
                                    nip, number_of_nodes, lvl-1, path)
                    except Exception, e:
                        logging.warning('P2P: find_nearest_exec' + \
                                str((nip, number_of_nodes, lvl)) + \
                                ' got Exception ' + repr(e))
                        resp = []
                else:
                    # The gnode of level 0 is a node.
                    # And it is participating.
                    resp = [[i] + me_as_gnode]
                sequence += resp
                number_of_nodes -= len(resp)
                if number_of_nodes <= 0:
                    break
        return sequence

    def find_nearest_send(self, to_gnode, nip, number_of_nodes, lvl, path):
      ''' Reach the gnode and ask to him... 
          to_gnode is part of a nip. It is a sequence of len = level-lvl
      '''

      start_tracking()
      try:
        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')
        # drop the request if number is too big
        if number_of_nodes > 100:
            raise Exception, 'find_nearest_nodes_to_nip: asked for too many nodes.'

        # Am I the destination?
        me_as_gnode = self.maproute.me[lvl:]
        if me_as_gnode == to_gnode:
            ret = self.find_nearest_exec(nip, number_of_nodes, lvl, path)
            return ret
        else:
            # Find the path to the destination
            complete_to_gnode = [None] * lvl + to_gnode
            n = self.neigh_get(complete_to_gnode)
            if n:
                ret = None
                execstr = 'ret = n.ntkd.p2p.PID_' + str(self.pid) + \
                '.find_nearest_send(to_gnode, nip, number_of_nodes, lvl, path)'
                # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
                #      by retrying after a while and up to a certain timeout.
                tc_max = xtime.TimeCapsule(20000) # give max 20 seconds to finish hook
                while True:
                    try:
                        exec(execstr)
                    except Exception, e:
                        if 'P2P is hooking' in e:
                            logging.warning('P2P routing: The neighbour is still hooking.')
                            if tc_max.get_ttl() < 0:
                                logging.warning('P2P routing: Giving up.')
                                raise e
                            else:
                                logging.warning('P2P routing: Wait a bit and try again.')
                                xtime.swait(1000)
                        else:
                            raise e
                    else:
                        break
                return ret
            else:
                logging.warning('I don\'t know to whom I must forward. ' + \
                                'Giving up. Raising exception.')
                raise Exception('Unreachable P2P destination ' + \
                                str(complete_to_gnode) + \
                                ' from ' + str(self.maproute.me) + '.')
      finally:
        stop_tracking()

    def find_nearest_up_and_down(self, center):
        ''' Starting from center and going up and down '''
        gsize = self.maproute.gsize
        ids = [center]
        from math import trunc
        times = trunc((gsize+1)/2)-1
        for to_add in xrange(1, times+1):
            for sign in [-1,1]:
                ids.append((center + to_add * sign) % gsize)
        if trunc(gsize / 2) * 2 == gsize:
            ids.append((center + (times+1) * -1) % gsize)
        return ids

    def find_nearest_up(self, center):
        ''' Starting from center and going only up '''
        gsize = self.maproute.gsize
        ids = [center]
        for to_add in xrange(1, gsize):
            ids.append((center + to_add) % gsize)
        return ids

    def find_nearest_down(self, center):
        ''' Starting from center and going only down '''
        gsize = self.maproute.gsize
        ids = [center]
        for to_add in xrange(1, gsize):
            ids.append((center - to_add) % gsize)
        return ids

    def msg_send(self, sender_nip, hip, msg):
        """Routes a packet to `hip'. Do not use this function directly, use
        self.peer() instead

        msg: it is a (func_name, args) pair."""

        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        logging.log(logging.ULTRADEBUG, 'P2P: msg_send '
                    'called by ' + str(sender_nip))
        lvl = self.maproute.nip_cmp(sender_nip)
        
        logging.log(logging.ULTRADEBUG, 'Someone is asking for P2P '
                                        'service to ' + str(hip))
        H_hip = self.H(hip)
        logging.log(logging.ULTRADEBUG, ' nearest known is ' + str(H_hip))
        if H_hip == self.maproute.me:
            # the msg has arrived
            logging.debug('I have been asked a P2P service, as the '
                          'nearest to ' + str(hip) + ' (msg=' + str(msg) +
                          ')')
            return self.msg_exec(sender_nip, msg)

        # forward the message until it arrives at destination
        n = self.neigh_get(H_hip)
        if n:
            logging.log(logging.ULTRADEBUG, ' through ' + str(n))
            ret = None
            execstr = 'ret = n.ntkd.p2p.PID_' + str(self.pid) + \
            '.msg_send(sender_nip, hip, msg)'
            logging.log(logging.ULTRADEBUG, 'Executing "' + execstr + 
                        '" ...')
            # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
            #      by retrying after a while and up to a certain timeout.
            tc_max = xtime.TimeCapsule(20000) # give max 20 seconds to finish hook
            while True:
                try:
                    exec(execstr)
                except Exception, e:
                    if 'P2P is hooking' in e:
                        logging.warning('P2P routing: The neighbour is still hooking.')
                        if tc_max.get_ttl() < 0:
                            logging.warning('P2P routing: Giving up.')
                            raise e
                        else:
                            logging.warning('P2P routing: Wait a bit and try again.')
                            xtime.swait(1000)
                    else:
                        raise e
                else:
                    break
            logging.log(logging.ULTRADEBUG, 'Executed "' + execstr + 
                        '". Returning ' + str(ret))
            return ret
        else:
            # Is it possible? Don't we retry?
            logging.warning('I don\'t know to whom I must forward. '
                            'Giving up. Raising exception.')
            raise Exception('Unreachable P2P destination ' + str(H_hip) + 
                            ' from ' + str(self.maproute.me) + '.')

    def call_msg_send_udp(self, neigh, sender_nip, hip, msg):
        """Use BcastClient to call msg_send"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'p2p.PID_' + 
                            str(self.pid) + '.msg_send_udp', 
                            (sender_nip, hip, msg))

    def msg_send_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid, 
                     sender_nip, hip, msg):
        """Returns the result of msg_send to remote caller.
           caller_id is the random value generated by the caller 
           for this call.
            It is replied back to the LAN for the caller to recognize a 
            reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated 
            to it.
           """

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if self.maproute.me == callee_nip and \
           self.neigh.netid == callee_netid:
            ret = None
            rpc.UDP_send_keepalive_forever_start(_rpc_caller, caller_id)
            try:
                logging.log(logging.ULTRADEBUG, 'calling msg_send...')
                # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
                #      by retrying after a while and up to a certain timeout.
                tc_max = xtime.TimeCapsule(20000) # give max 20 seconds to finish hook
                while True:
                    try:
                        ret = self.msg_send(sender_nip, hip, msg)
                    except Exception, e:
                        if 'P2P is hooking' in e:
                            logging.warning('P2P routing: The neighbour is still hooking.')
                            if tc_max.get_ttl() < 0:
                                logging.warning('P2P routing: Giving up.')
                                raise e
                            else:
                                logging.warning('P2P routing: Wait a bit and try again.')
                                xtime.swait(1000)
                        else:
                            raise e
                    else:
                        break
                logging.log(logging.ULTRADEBUG, 'returning ' + str(ret))
            except Exception as e:
                ret = ('rmt_error', str(e))
                logging.warning('msg_send_udp: returning exception ' + 
                                str(ret))
            finally:
                rpc.UDP_send_keepalive_forever_stop(caller_id)
            logging.log(logging.ULTRADEBUG, 'calling UDP_send_reply...')
            rpc.UDP_send_reply(_rpc_caller, caller_id, ret)

    def msg_exec(self, sender_nip, msg):
        ret = self.dispatch(CallerInfo(), *msg)
        if isinstance(ret, tuple) and len(ret) == 2 and ret[0] == 'rmt_error':
            raise RPCError(ret[1])
        return ret

    class RmtPeer(FakeRmt):
        def __init__(self, p2p, hIP=None, key=None, neigh=None):
            self.p2p = p2p
            self.key = key
            self.hIP = hIP
            self.neigh = neigh
            FakeRmt.__init__(self)

        def evaluate_hash_nip(self):
            if self.hIP is None:
                self.hIP = self.p2p.h(self.key)
            if self.hIP is None:
                raise Exception, "'key' does not map to a IP."

        def get_hash_nip(self):
            self.evaluate_hash_nip()
            return self.hIP

        def rmt(self, func_name, *params):
            """Overrides FakeRmt.rmt()"""
            self.evaluate_hash_nip()
            if self.neigh:
                # We are requested to use this one as first hop via UDP.
                logging.log(logging.ULTRADEBUG, 'P2P: Use UDP via ' + 
                            str(self.neigh) + ' to reach peer.')
                return self.p2p.call_msg_send_udp(self.neigh, 
                                                  self.p2p.maproute.me, 
                                                  self.hIP, 
                                                  (func_name, params,))
            else:
                # Use TCP version.
                logging.log(logging.ULTRADEBUG, 'P2P: Use TCP to reach peer.')
                return self.p2p.msg_send(self.p2p.maproute.me, self.hIP, 
                                         (func_name, params,))

    def peer(self, hIP=None, key=None, neigh=None):
        if hIP is None and key is None:
                raise Exception, ("hIP and key are both None. "
                                  "Specify at least one")
        return self.RmtPeer(self, hIP=hIP, key=key, neigh=neigh)

    
class OptionalP2P(P2P):
    """This is the class that must be inherited to create a P2P module.
    """

    def __init__(self, ntkd_status, radar, maproute, pid):
        """radar, maproute: the instances of the relative modules

           pid: P2P id of the service associated to this map
        """

        P2P.__init__(self, ntkd_status, radar, maproute, pid)

        self.mapp2p = MapP2P(self.maproute.levels,
                             self.maproute.gsize,
                             self.maproute.me,
                             self.pid)

        self.maproute.events.listen('ME_CHANGED', self.mapp2p.change_nip)
        self.maproute.events.listen('NODE_DELETED', self.mapp2p.node_dead)

        # are we a participant?
        self.participant = False

        self.remotable_funcs += [self.participant_add,
                                 self.participant_add_udp]

    def is_participant(self, lvl, idn):
        """Returns True iff the node lvl,idn is participating
        to the service.
        """
        return self.mapp2p.node_get(lvl, idn).participant

    def repr_participants(self):
        """Returns a representation of the map of participants.
        """
        return self.mapp2p.repr_me()

    def participate(self):
        """Let's become a participant node"""
        self.participant = True
        self.mapp2p.participate()
        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        logging.log(logging.ULTRADEBUG, 'P2P: I participate. I have in my network ' + str(len(current_nr_list)) + ' neighbours.')

        # TODO handle the case where one of neighbours does not reply
        # (raises an error)
        for nr in current_nr_list:
            try:
                logging.debug('calling participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
                self.call_participant_add_udp(nr, self.maproute.me)
                logging.debug('done calling participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
            except Exception, e:
                logging.debug('error ' + repr(e) + ' calling '
                              'participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))

    def sit_out(self):
        """Let's go outside and don't participate to the service """
        self.participant = False
        self.mapp2p.sit_out()
        current_nr_list = self.neigh.neigh_list(in_my_network=True)

        # TODO handle the case where one of neighbours does not reply 
        # (raises an error)
        for nr in current_nr_list:
            try:
                logging.debug('calling participant_del(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
                self.call_participant_del_udp(nr, self.maproute.me)
                logging.debug('done calling participant_del(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
            except Exception, e:
                logging.debug('error ' + repr(e) + ' calling '
                              'participant_del(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
    
    def call_participant_add_udp(self, neigh, pIP):
        """Use BcastClient to call participant_add_udp"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        logging.log(logging.ULTRADEBUG, 'P2P: Calling participant_add_udp ' +
                    str(nip))
        return rpc.UDP_call(nip, netid, devs, 'p2p.PID_'+str(self.mapp2p.pid)+
                            '.participant_add_udp', (pIP,))

    def call_participant_del_udp(self, neigh, pIP):
        """Use BcastClient to call participant_del_udp"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        logging.log(logging.ULTRADEBUG, 'P2P: Calling participant_del_udp ' +
                    str(nip))
        return rpc.UDP_call(nip, netid, devs, 'p2p.PID_'+str(self.mapp2p.pid)+
                        '.participant_del_udp', (pIP,))

    def participant_add_udp(self, _rpc_caller, caller_id, callee_nip, 
                            callee_netid, pIP):
        """Returns the result of participant_add to remote caller.
           caller_id is the random value generated by the caller for 
            this call.
            It is replied back to the LAN for the caller to recognize a reply
            destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated to 
            it.
           """
        logging.log(logging.ULTRADEBUG, 'P2P: participant_add_udp '
                    'called by ' + str(pIP))
        lvl = self.maproute.nip_cmp(pIP)
        if self.maproute.me == callee_nip and \
             self.neigh.netid == callee_netid:
            self.participant_add(pIP)
            # Since it is micro, I will reply None
            rpc.UDP_send_reply(_rpc_caller, caller_id, None)

    def participant_del_udp(self, _rpc_caller, caller_id, callee_nip, 
                            callee_netid, pIP):
        """Returns the result of participant_del to remote caller.
           caller_id is the random value generated by the caller 
           for this call.
            It is replied back to the LAN for the caller to recognize 
            a reply destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request 
            destinated to it.
           """
        logging.log(logging.ULTRADEBUG, 'P2P: participant_del_udp '
                    'called by ' + str(pIP))
        lvl = self.maproute.nip_cmp(pIP)
        if self.maproute.me == callee_nip and \
             self.neigh.netid == callee_netid:
            self.participant_del(pIP)
            # Since it is micro, I will reply None
            rpc.UDP_send_reply(_rpc_caller, caller_id, None)

    @microfunc(True)
    def participant_add(self, pIP):
        '''Add a participant node to the P2P service

        :param pIP: participant node's Netsukuku IP (nip)
        '''

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        continue_to_forward = False
        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        mp  = self.mapp2p
        lvl = self.maproute.nip_cmp(pIP, mp.me)
        for l in xrange(lvl, mp.levels):
            # We might receive the request to register a participant from a
            # neighbour that has not yet sent us an ETP. In that case we would
            # not have yet the route to it in the map. It's a matter of time,
            # so we wait. (we are in a microfunc)
            while self.maproute.node_get(l, pIP[l]).is_free():
                xtime.swait(100)
            if not mp.node_get(l, pIP[l]).participant:
                logging.debug('registering participant (%s, %s) to '
                              'service %s.' % (l, pIP[l], mp.pid))
                mp.participant_node_add(l, pIP[l])
                continue_to_forward = True

        if not continue_to_forward:
            return

        # continue to advertise the new participant

        # TODO handle the case where one of neighbours does not reply 
        # (raises an error)
        # TODO do we have to skip the one who sent to us? It is not 
        # needed cause it won't forward anyway.

        for nr in current_nr_list:
            try:
                logging.debug('forwarding participant_add(%s) to '
                              '%s service %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip), mp.pid))
                self.call_participant_add_udp(nr, pIP)
                logging.debug('done forwarding participant_add(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))
            except Exception, e:
                logging.debug('timeout (no problem) forwarding '
                              'participant_add(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))

    @microfunc(True)
    def participant_del(self, pIP):
        ''' Remove a participant node from the P2P service
        
        :param pIP: participant node's Netsukuku IP (nip)
        '''

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        continue_to_forward = False
        current_nr_list = self.neigh.neigh_list(in_my_network=True)
        mp  = self.mapp2p
        lvl = self.maproute.nip_cmp(pIP, mp.me)
        for l in xrange(lvl, mp.levels):
            if mp.node_get(l, pIP[l]).participant:
                logging.debug('unregistering participant (%s, %s) '
                              'to service %s.' % (l, pIP[l], mp.pid))
                mp.participant_node_del(l, pIP[l])
                continue_to_forward = True

        if not continue_to_forward:
            return

        # continue to advertise the new participant

        # TODO handle the case where one of neighbours does not reply 
        #   (raises an error)
        # TODO do we have to skip the one who sent to us? It is not needed
        #   cause it won't forward anyway.
        
        for nr in current_nr_list:
            try:
                logging.debug('forwarding participant_del(%s) to '
                              '%s service %s.' %
                              (pIP, self.maproute.ip_to_nip(nr.ip), mp.pid))
                self.call_participant_del_udp(nr, pIP)
                logging.debug('done forwarding participant_del(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))
            except Exception, e:
                logging.debug('timeout (no problem) forwarding '
                              'participant_del(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))
        
class P2PAll(object):
    """Class of all the registered P2P services"""

    __slots__ = ['ntkd_status',
                 'radar',
                 'neigh',
                 'maproute',
                 'service',
                 'remotable_funcs',
                 'events',
                 'micro_to_kill']

    def __init__(self, ntkd_status, radar, maproute):

        self.ntkd_status = ntkd_status
        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute

        self.service = {}

        self.micro_to_kill = {}

        self.remotable_funcs = [self.get_optional_participants]
        self.events=Event(['P2P_HOOKED'])

    def pid_add(self, pid):
        logging.log(logging.ULTRADEBUG, 'Called P2PAll.pid_add...')
        self.service[pid] = P2P(self.radar, self.maproute, pid)
        return self.service[pid]

    def pid_del(self, pid):
        if pid in self.service:
            del self.service[pid]

    def pid_get(self, pid, strict=False):
        if pid not in self.service:
            return self.pid_add(pid)
        else:
            return self.service[pid]

    def get_optional_participants(self):
        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        return [(s, self.service[s].mapp2p.map_data_pack())
                    for s in self.service
                        if isinstance(self.service[s], OptionalP2P)]

    def p2p_register(self, p2p):
        """Used to add for the first time a P2P instance of a module in the
           P2PAll dictionary."""

        logging.log(logging.ULTRADEBUG, 'Called P2PAll.p2p_register for ' + 
                    str(p2p.pid) + '...')
        # It's possible that the stub P2P instance `self.pid_get(p2p.pid)'
        # created by pid_add() has an update map of participants, which has
        # been accumulated during the time. Copy this map in the `p2p'
        # instance to be sure.
        if p2p.pid in self.service:
            logging.log(logging.ULTRADEBUG, 'Called P2PAll.p2p_register '
                        'for ' + str(p2p.pid) + '... cloning...')
            if not isinstance(self.pid_get(p2p.pid), StrictP2P):
                map_pack = self.pid_get(p2p.pid).mapp2p.map_data_pack()
                p2p.mapp2p.map_data_merge(map_pack)
        self.service[p2p.pid] = p2p

    @microfunc(True, keep_track=1)
    def p2p_hook(self, *args):
        """P2P hooking procedure

        It gets the P2P maps from our nearest neighbour"""
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'p2p_hook' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['p2p_hook'])
        try:
            self.micro_to_kill['p2p_hook'] = micro_current()

            logging.debug('P2P hooking: started')
            logging.log(logging.ULTRADEBUG, 'P2P hooking: My actual list of '
                        'optional services is: ' + str(self.log_services()))

            neighs_in_net = self.neigh.neigh_list(in_my_network=True)
            got_answer = False
            try_again = True
            nrmaps_pack = None
            while (not got_answer) and try_again:
                ## Find our nearest neighbour
                minlvl = self.maproute.levels
                minnr = None
                for nr in neighs_in_net:
                    lvl = self.maproute.nip_cmp(self.maproute.ip_to_nip(nr.ip))
                    if lvl < minlvl:
                        minlvl = lvl
                        minnr  = nr

                if minnr is None:
                    # nothing to do
                    logging.debug('P2P hooking: No neighbours '
                                'to ask for the list of optional services.')
                    try_again = False
                else:
                    logging.log(logging.ULTRADEBUG, 'P2P hooking: I will ask for the '
                                'list of optional services to ' + str(minnr))
                    try:
                        nrmaps_pack = minnr.ntkd.p2p.get_optional_participants()
                        logging.log(logging.ULTRADEBUG, 'P2P hooking: ' + str(minnr) + 
                                    ' answers ' + str(nrmaps_pack))
                        got_answer = True
                    except Exception, e:
                        logging.warning('P2P hooking: Asking to ' + str(minnr) + 
                                        ' failed.')
                        neighs_in_net.remove(minnr)

            if got_answer:
                for (pid, map_pack) in nrmaps_pack:
                    self.pid_get(pid).mapp2p.map_data_merge(map_pack)

            # The re-participation is delayed up to a
            #  moment of choice of the specific service.
            #  Eg the andna module will call self.participate after the
            #  completion of andna_hook.
            for s, obj in self.service.items():
                self.service[s].exit_wait_p2p_hook()

            logging.debug('P2P hooking: My final list of '
                        'optional services is: ' + str(self.log_services()))

            logging.info('P2P: Emit signal P2P_HOOKED.')
            self.events.send('P2P_HOOKED', ())

        finally:
            del self.micro_to_kill['p2p_hook']

    def __getattr__(self, str):

        if str[:4] == "PID_":
            return self.pid_get(int(str[4:]))
        raise AttributeError

    def log_services(self):
        return [(s, self.service[s].mapp2p.repr_me())
                    for s in self.service
                        if isinstance(self.service[s], OptionalP2P)]

