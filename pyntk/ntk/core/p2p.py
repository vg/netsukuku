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

        self.remotable_funcs = [self.msg_send_udp,
                                self.msg_deliver,
                                self.find_nearest,
                                self.number_of_participants]

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

    ########## Helper functions for "routed" methods.

    def neigh_get_from_lvl_id(self, lvl, id):
        """Returns the Neigh instance of the neighbour we must use to reach
           the hash node.

        `hip' is the IP of the hash node.
        If nothing is found, None is returned
        """

        br = self.maproute.node_get(lvl, id).best_route()
        # TODO choose the best route that does not contain the
        #      gnode from which we received the message, or better
        #      any of the gnodes already touched by the message.

        if not br:
            return None
        return br.gw

    def neigh_get_from_hip(self, hip):
        lvl = self.maproute.nip_cmp(hip, self.maproute.me)
        return self.neigh_get_from_lvl_id(lvl, hip[lvl])

    def list_ids(self, center, sign):
        ''' Starting from center and going in one direction '''
        gsize = self.maproute.gsize
        ids = []
        for to_add in xrange(gsize):
            ids.append((center + to_add * sign) % gsize)
        return ids

    def search_participant(self, hIP, path_sign=1):
        # It was H
        mp = self.maproute
        H_hIP = [None] * mp.levels
        for l in reversed(xrange(mp.levels)):
            for hid in self.list_ids(hIP[l], path_sign):
                if self.is_participant(l, hid):
                    H_hIP[l] = hid
                    break
            if H_hIP[l] is None:
                raise Exception, 'No participants'
            if H_hIP[l] != mp.me[l]:
                # we must stop here
                break
        if H_hIP[l] == mp.me[l]:
            lvl, id = -1, None
        else:
            lvl, id = l, H_hIP[l]
        pass # logging.log(logging.ULTRADEBUG, 'search_participant' + \
        #        str((hIP, path_sign)) + ' = ' + str((lvl, id)))
        return lvl, id

    def search_participant_as_nip(self, hIP, path_sign=1):
        # It was H
        mp = self.maproute
        lvl, id = self.search_participant(hIP, path_sign)
        ret = self.maproute.me[:]
        if lvl >= 0:
            ret[lvl] = id
            ret[:lvl] = [None] * (lvl)
        return ret

    def execute_p2p_tpl(self, p2p_tpl):
        '''Avoid loops, neverending stories, and the like'''
        # When a message is routed through the net by the p2p module,
        # we keep track of the path walked.
        # p2p_tpl = [tpl, tc, obj]
        #  obj is reserved for future uses.
        #  tpl = sequence<sequence<id>>
        #   tpl has always 'levels' elements.
        #   tpl[0] is a sequence of ids of level 'levels'
        #   tpl[1] is a sequence of ids of level 'levels-1'
        #   ...
        #   tpl[levels] is a sequence of ids of level '0'
        #   The last id of each level form the nip of the last hop
        #  tc is None or a TimeCapsule. When it expires the message is dropped.
        # There are several functions in module p2p that try to
        # route messages, such as msg_deliver, find_nearest,
        # number_of_participants, and so on. Each function receives
        # as a parameter a "p2p_tpl" instance, "passes" it through this
        # method -- eg: p2p_tpl = self.execute_p2p_tpl(p2p_tpl)
        # and then passes it to the next hop.
        # The first caller can pass a None as p2p_tpl.

        if p2p_tpl is None:
            tpl, tc, obj = self.first_p2p_tpl(xtime.TimeCapsule(120000), None)
        elif len(p2p_tpl) == 2:
            tpl, tc, obj = self.first_p2p_tpl(p2p_tpl[0], p2p_tpl[1])
        else:
            tpl, tc, obj = p2p_tpl
            # check timeout
            if isinstance(tc, xtime.TimeCapsule):
                if tc.get_ttl() < 0:
                    # drop
                    raise ExpectableException, 'P2P message timed out.'
            # I check whether the message did take a loop. If so, wait a bit before proceeding.
            tpl, loop = self.convert_tpl(tpl)
            if loop:
                logging.log(logging.ULTRADEBUG, 'P2P Routing: loop detected. wait a bit.')
                xtime.swait(2000)
        return [tpl, tc, obj]

    def convert_tpl(self, tpl):
        # tpl is valid for someone else.
        from_nip = [ids[-1] for ids in reversed(tpl)]
        last_lvl = self.maproute.nip_cmp(from_nip)
        unchanged = tpl[:-last_lvl]
        # detect the loop
        last_path = unchanged[-1]
        loop_detected = False
        if self.maproute.me[last_lvl] in last_path:
            loop_detected = True
            pos = last_path.index(self.maproute.me[last_lvl])
            last_path[pos:] = []
        # add my part
        last_path.append(self.maproute.me[last_lvl])
        new_part = [[n] for n in reversed(self.maproute.me[:last_lvl])]
        return unchanged + new_part, loop_detected

    def first_p2p_tpl(self, tc, obj):
        return [[[n] for n in reversed(self.maproute.me)], tc, obj]

    ########## Sending of messages.

    def msg_send(self, sender_nip, hip, msg):
        ''' Start a tentative to send a message to a certain
            perfect hip
        '''
        return self.msg_deliver(None, sender_nip, \
               hip, msg)

    def msg_deliver(self, p2p_tpl, sender_nip, hip, msg, tc_max=None):
      start_tracking()
      try:
        # I have to deliver a msg to the nearest to hip.

        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        # Called by any routing function in module p2p
        p2p_tpl = self.execute_p2p_tpl(p2p_tpl)

        prox_lvl, prox_id = self.search_participant(hip)
        
        # If the node is exactly me, I execute the message
        if prox_lvl == -1:
            return self.msg_exec(sender_nip, msg)
        else:
            # route
            return self.route_msg_deliver(p2p_tpl, sender_nip, hip, prox_lvl, prox_id, msg)
      finally:
        stop_tracking()

    def route_msg_deliver(self, p2p_tpl, sender_nip, hip, lvl, id, msg):
        ret = None
        execstr = 'ret = n.ntkd.p2p.PID_' + str(self.pid) + \
                  '.msg_deliver(p2p_tpl, sender_nip, hip, msg)'
        logging.log(logging.ULTRADEBUG, 'Executing "' + execstr + \
                    '" ...')
        # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
        #     and ZombieException('I am a zombie.')
        #     and Unreachable P2P destination
        #      by retrying after a while and up to a certain timeout.
        tc_max = xtime.TimeCapsule(60000) # give max 60 seconds to finish hook
        while True:
            last_e = None
            done = False
            logging.debug('P2P routing: route_msg_deliver to ' + str((lvl, id)))
            n = self.neigh_get_from_lvl_id(lvl, id)
            if n:
                try:
                    exec(execstr)
                    done = True
                except Exception, e:
                    if 'P2P is hooking' in e or 'I am a zombie' in e:
                        logging.debug('P2P routing: got ' + str(e))
                        last_e = e
                    else:
                        raise e
            else:
                logging.debug('P2P routing: unknown route.')
            if done:
                break
            if tc_max.get_ttl() < 0:
                logging.warning('P2P routing: Too many errors. Giving up.')
                if last_e:
                    raise last_e
                else:
                    raise Exception, 'Unreachable P2P destination.'
            else:
                logging.debug('P2P routing: Temporary failure. Wait a bit and try again.')
                xtime.swait(1000)
        logging.log(logging.ULTRADEBUG, 'Executed "' + execstr + \
                    '". Returning ' + str(ret))
        return ret

    ########## Helper functions for UDP.

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
                #    and ZombieException('I am a zombie.')
                #      by retrying after a while and up to a certain timeout.
                tc_max = xtime.TimeCapsule(60000) # give max 60 seconds to finish hook
                while True:
                    try:
                        ret = self.msg_send(sender_nip, hip, msg)
                    except Exception, e:
                        if 'P2P is hooking' in e or 'I am a zombie' in e:
                            if tc_max.get_ttl() < 0:
                                logging.warning('P2P routing: Too long. Giving up.')
                                raise e
                            else:
                                logging.warning('P2P routing: Temporary failure. Wait a bit and try again.')
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

    ########## Execution of messages.

    def msg_exec(self, sender_nip, msg):
        ret = self.dispatch(CallerInfo(), *msg)
        if isinstance(ret, tuple) and len(ret) == 2 and ret[0] == 'rmt_error':
            raise RPCError(ret[1])
        return ret

    ########## Search functions for registration with replica. Routed.

    def find_nearest_to_register(self, nip, num_dupl):
        return self.find_nearest(None, nip, num_dupl, self.maproute.levels, 0)

    def find_nearest(self, p2p_tpl, nip, num_dupl, lvl, id):
      start_tracking()
      try:

        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        # Called by any routing function in module p2p
        p2p_tpl = self.execute_p2p_tpl(p2p_tpl)

        return self.inside_find_nearest(p2p_tpl, nip, num_dupl, lvl, id)
      finally:
        stop_tracking()

    def inside_find_nearest(self, p2p_tpl, nip, num_dupl, lvl, id):

        if num_dupl == 0:
            return []
        if lvl < self.maproute.levels:
            if lvl == 0:
                ret = [[id] + self.maproute.me[1:]] \
                        if self.is_participant(0, id) \
                        else []
                return ret
            if not self.is_participant(lvl, id):
                return []
        # (lvl, id) is a participant g-node. To know the remaining 
        # num_dupl nodes nearest to nip and participating inside it,
        # if I am in this g-node I do a
        # step down in the level, otherwise I have to route the
        # question to the g-node.
        if lvl == self.maproute.levels or self.maproute.me[lvl] == id:
            pass # logging.log(logging.ULTRADEBUG, 'find_nearest go down')
            # down one level
            sequence = []
            new_lvl = lvl - 1
            for new_id in self.list_ids(nip[new_lvl], 1):
                pass # logging.log(logging.ULTRADEBUG, 'find_nearest calling...')
                sequence += self.inside_find_nearest(p2p_tpl, nip, num_dupl-len(sequence), new_lvl, new_id)
                if len(sequence) >= num_dupl:
                    break
            return sequence
        else:
            return self.route_find_nearest(p2p_tpl, nip, num_dupl, lvl, id)

    def route_find_nearest(self, p2p_tpl, nip, num_dupl, lvl, id):
        # route the request
        ret = None
        execstr = 'ret = n.ntkd.p2p.PID_' + str(self.pid) + \
                  '.find_nearest(p2p_tpl, nip, num_dupl, lvl, id)'
        logging.log(logging.ULTRADEBUG, 'Executing "' + \
                    execstr + '" ...')
        # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
        #     and ZombieException('I am a zombie.')
        #     and Unreachable P2P destination
        #      by retrying after a while and up to a certain timeout.
        tc_max = xtime.TimeCapsule(60000) # give max 60 seconds to finish hook
        while True:
            last_e = None
            done = False
            logging.debug('P2P routing: route_find_nearest to ' + str(nip) + ' in ' + str((lvl, id)))
            n = self.neigh_get_from_lvl_id(lvl, id)
            if n:
                try:
                    exec(execstr)
                    done = True
                except Exception, e:
                    if 'P2P is hooking' in e or 'I am a zombie' in e:
                        logging.debug('P2P routing: got ' + str(e))
                        last_e = e
                    else:
                        raise e
            else:
                logging.debug('P2P routing: unknown route.')
            if done:
                break
            if tc_max.get_ttl() < 0:
                logging.warning('P2P routing: Too many errors. Giving up.')
                if last_e:
                    raise last_e
                else:
                    raise Exception, 'Unreachable P2P destination.'
            else:
                logging.warning('P2P routing: Temporary failure. Wait a bit and try again.')
                xtime.swait(1000)
        logging.log(logging.ULTRADEBUG, 'Executed "' + \
                    execstr + '". Returning ' + str(ret))
        return ret

    ########## Search functions for hooking phase. Routed

    def get_number_of_participants(self, lvl, id, timeout):
        p2p_tpl = [xtime.TimeCapsule(timeout), None]
        return self.number_of_participants(p2p_tpl, lvl, id)

    def number_of_participants(self, p2p_tpl, lvl, id):
      start_tracking()
      try:

        if self.wait_p2p_hook:
            raise P2PHookingError, 'P2P is hooking. Request not valid.'

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        # Called by any routing function in module p2p
        p2p_tpl = self.execute_p2p_tpl(p2p_tpl)

        return self.inside_number_of_participants(p2p_tpl, lvl, id)
      finally:
        stop_tracking()

    def inside_number_of_participants(self, p2p_tpl, lvl, id):

        if lvl == 0:
            return 1 if self.is_participant(0, id) else 0
        if not self.is_participant(lvl, id):
            return 0
        # (lvl, id) is a participant g-node. To know the number of
        # nodes participating inside it, if I am in this g-node I do a
        # step down in the level, otherwise I have to route the
        # question to the g-node.
        if self.maproute.me[lvl] == id:
            # down one level
            pass # print 'down one level'
            new_lvl = lvl - 1
            ret = sum([self.inside_number_of_participants(p2p_tpl, new_lvl, new_id) \
                        for new_id in xrange(self.maproute.gsize)])
            pass # print 'returning', ret
            return ret
        else:
            return self.route_number_of_participants(p2p_tpl, lvl, id)

    def route_number_of_participants(self, p2p_tpl, lvl, id):
        # route the request
        ret = None
        execstr = 'ret = n.ntkd.p2p.PID_' + str(self.pid) + \
                  '.number_of_participants(p2p_tpl, lvl, id)'
        logging.log(logging.ULTRADEBUG, 'Executing "' + \
                    execstr + '" ...')
        # Handles P2PHookingError, 'P2P is hooking. Request not valid.'
        #     and ZombieException('I am a zombie.')
        #     and Unreachable P2P destination
        #      by retrying after a while and up to a certain timeout.
        tc_max = xtime.TimeCapsule(60000) # give max 60 seconds to finish hook
        while True:
            last_e = None
            done = False
            logging.debug('P2P routing: route_number_of_participants in ' + str((lvl, id)))
            n = self.neigh_get_from_lvl_id(lvl, id)
            if n:
                try:
                    exec(execstr)
                    done = True
                except Exception, e:
                    if 'P2P is hooking' in e or 'I am a zombie' in e:
                        logging.debug('P2P routing: got ' + str(e))
                        last_e = e
                    else:
                        raise e
            else:
                logging.debug('P2P routing: unknown route.')
            if done:
                break
            if tc_max.get_ttl() < 0:
                logging.warning('P2P routing: Too many errors. Giving up.')
                if last_e:
                    raise last_e
                else:
                    raise Exception, 'Unreachable P2P destination.'
            else:
                logging.warning('P2P routing: Temporary failure. Wait a bit and try again.')
                xtime.swait(1000)
        logging.log(logging.ULTRADEBUG, 'Executed "' + \
                    execstr + '". Returning ' + str(ret))
        return ret

    ########## Helper functions for hooking phase.

    def find_hook_peers(self, lvl, num_dupl, timeout=10000):
        remaining = num_dupl
        first_back_id = None
        last_back_id = None
        nip_to_reach_first_forward = self.maproute.me[:]
        nip_to_reach_first_forward[lvl] = \
                  (nip_to_reach_first_forward[lvl] + 1) \
                  % self.maproute.gsize
        ids = self.list_ids(self.maproute.me[lvl], -1)
        # not interested in myself
        ids = ids[1:]
        for _id in ids:
            pass # print 'find_hook_peers: call get_number_of_participants', (lvl, _id)
            num = 0
            try:
                num = self.get_number_of_participants(lvl, _id, timeout)
            except:
                pass
            pass # print 'find_hook_peers: get_number_of_participants', (lvl, _id), 'is', num
            if num > 0 and first_back_id is None: first_back_id = _id
            remaining -= num
            if remaining <= 0:
                last_back_id = _id
                break
        if remaining > 0:
            # not enough nodes
            if first_back_id is None:
                # no nodes at all
                return None, None, None
            else:
                return nip_to_reach_first_forward, None, None
        nip_to_reach_first_back = self.maproute.me[:]
        nip_to_reach_last_back = self.maproute.me[:]
        nip_to_reach_first_back[lvl] = first_back_id
        nip_to_reach_last_back[lvl] = last_back_id
        for _lvl in xrange(lvl):
            nip_to_reach_last_back[_lvl] = \
                  (nip_to_reach_last_back[_lvl] + 1) \
                  % self.maproute.gsize
        return nip_to_reach_first_forward, nip_to_reach_first_back, nip_to_reach_last_back

    def is_x_after_me_for_y(self, x, y):
        # x is a nip, y is a nip, self.maproute.me is a nip.
        # y is the perfect nip for a particular key.
        # If someone asks for y, will he hit me before x?
        i = self.maproute.nip_cmp(x)
        ids = self.list_ids(y[i], 1)
        return ids.index(self.maproute.me[i]) < ids.index(x[i])

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

