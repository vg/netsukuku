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
# Implementation of the P2P Over Ntk RFC. See {-P2PNtk-}
#

import time

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime

from ntk.core.map import Map
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.log import get_stackframes
from ntk.lib.micro import microfunc, Channel, micro_block
from ntk.lib.rencode import serializable
from ntk.lib.rpc import FakeRmt, RPCDispatcher, CallerInfo


class P2PError(Exception):
    '''Generic P2P Error'''

class ParticipantNode(object):
    def __init__(self, lvl, id, participant=False, its_me=False):
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
        # de-serialization. So use the value that will produce the 
        # smaller output with rencode.dumps.
        # TODO test what this value is... perhaps None is better than 0 ?
        return (0, 0, self.participant)

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
        if self.node_get(lvl, id).is_free():
                self.node_get(lvl, id).participant = True
                self.node_add(lvl, id)
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P updated: ' + 
                    str(self.repr_me()))

    def me_changed(self, old_me, new_me):
        '''Changes self.me

        :param old_me: my old nip (not used in MapP2P)
        :param new_me: new nip
        '''
        Map.me_change(self, new_me)
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P updated after '
                               'me_changed: ' + str(self.repr_me()))

    @microfunc(True)
    def node_del(self, lvl, id):
        Map.node_del(self, lvl, id)

    def participate(self):
        """Set self.me to be a participant node."""

        for l in xrange(self.levels):
            self.participant_node_add(l, self.me[l])

    def map_data_pack(self):
        """Prepares a packed_mapp2p to be passed to mapp2p.map_data_merge
        in another host."""
        def fmake_participant(node):
            node.participant = True
        return Map.map_data_pack(self, fmake_participant)

    def map_data_merge(self, (nip, plist, nblist)):
        """Copies a mapp2p from another nip's point of view."""
        logging.log(logging.ULTRADEBUG, 'Merging a mapp2p.map_data_merge: '
                    'before: ' + self.repr_me())
        # Was I participant?
        me_was = [False] * self.levels
        for lvl in xrange(self.levels):
            me_was[lvl] = self.node_get(lvl, self.me[lvl]).participant
        # Merge as usual...
        lvl=self.nip_cmp(nip, self.me)
        logging.log(logging.ULTRADEBUG, 'Merging a mapp2p at level ' + 
                    str(lvl))
        logging.log(logging.ULTRADEBUG, get_stackframes(back=1))
        Map.map_data_merge(self, (nip, plist, nblist))
        # ... ripristine myself.
        for lvl in xrange(self.levels):
            if me_was[lvl]:
                self.participant_node_add(lvl, self.me[lvl])
        logging.log(logging.ULTRADEBUG, 'Merging a mapp2p.map_data_merge: '
                    'after: ' + self.repr_me())

    def repr_me(self, func_repr_node=None):
        def repr_node_mapp2p(node):
            if node.participant: return 'X'
            return ' '
        if func_repr_node is None: func_repr_node = repr_node_mapp2p
        return Map.repr_me(self, func_repr_node)


# `msg_id' is just a counter that is incremented and attached to the
# message that we are sending. In this way, the remote node can 
# read all the messages sent in the correct order.
# We need to store the last `msg_id' received from each node into 
# `msg_id_table' to check the received messages validity.

msg_id = 0
msg_id_table = {}
        
def check_ids(ip, id):
    """ Check the current id validity on the basis of the previous one """
    global msg_id_table
    if not ip in msg_id_table:
        msg_id_table[ip] = 0
    previous = msg_id_table[ip]
    logging.log(logging.ULTRADEBUG, 'msg_id = ' + str(id) + 
                                    ' previous = ' + str(previous))
    if id >= previous:
        msg_id_table[ip] = id
        return True
    return False

def updated_id():
    """ Increments and return the current message id """
    global msg_id
    msg_id += 1
    return msg_id

    
class P2P(RPCDispatcher):
    """This is the class that must be inherited to create a P2P module.
    """

    def __init__(self, radar, maproute, pid):
        """radar, maproute: the instances of the relative modules

           pid: P2P id of the service associated to this map
        """

        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        self.chan_replies = Channel()
                
        self.mapp2p = MapP2P(self.maproute.levels,
                             self.maproute.gsize,
                             self.maproute.me,
                             pid)

        self.maproute.events.listen('ME_CHANGED', self.me_changed)
        self.maproute.events.listen('NODE_DELETED', self.mapp2p.node_del)

        # are we a participant?
        self.participant = False

        self.remotable_funcs = [self.participant_add,
                                self.participant_add_udp,
                                self.msg_send,
                                self.msg_send_udp]

        RPCDispatcher.__init__(self, root_instance=self)

    @microfunc()
    def me_changed(self, old_me, new_me):
        """My nip has changed."""
        self.mapp2p.me_changed(old_me, new_me)
        self.re_participate()

    def h(self, key):
        """This is the function h:KEY-->hIP.

        You should override it with your own mapping function.
        """
        return key

    def H(self, hIP):
        """This is the function that maps each IP to an existent hash node IP
           If there are no participants, None is returned"""
        mp = self.mapp2p
        logging.log(logging.ULTRADEBUG, 'H: H(' + str(hIP) + ')')
        logging.log(logging.ULTRADEBUG, 'H: mapp2p = ' + mp.repr_me())
        H_hIP = [None] * mp.levels
        for l in reversed(xrange(mp.levels)):
            for id in xrange(mp.gsize):
                for sign in [-1,1]:
                    hid=(hIP[l] + id * sign) % mp.gsize
                    if mp.node_get(l, hid).participant:
                        H_hIP[l] = hid
                        break
                if H_hIP[l] is not None:
                    break
            if H_hIP[l] is None:
                logging.log(logging.ULTRADEBUG, 'H: H(' + str(hIP) + 
                            ') = None')
                return None

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

        if not br:
            return None
        return br.gw

    def re_participate(self, *args):
        """Let's become a participant node again. Used when my nip 
        has changed."""
        if self.participant:
            self.participate()
        logging.log(logging.ULTRADEBUG, 'P2P: MapP2P updated after '
                    're_participate: ' + str(self.mapp2p.repr_me()))

    def participate(self):
        """Let's become a participant node"""
        self.participant = True
        self.mapp2p.participate()
        current_nr_list = self.neigh.neigh_list(in_my_network=True)

        # TODO handle the case where one of neighbours does not reply
        # (raises an error)
        for nr in current_nr_list:
            try:
                logging.debug('calling participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
                self.call_participant_add_udp(nr, self.maproute.me,
                                              updated_id())
                logging.debug('done calling participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))
            except:
                logging.debug('timeout (no problem) calling '
                              'participant_add(myself) to %s.' % 
                              self.maproute.ip_to_nip(nr.ip))

    def call_participant_add_udp(self, neigh, pIP, msg_id):
        """Use BcastClient to call etp_exec"""
        devs = [neigh.bestdev[0]]
        nip = self.ntkd.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        logging.log(logging.ULTRADEBUG, 'P2P: Calling participant_add_udp ' +
                    str(nip) + ' with msg_id = ' + str(msg_id))
        return rpc.UDP_call(nip, netid, devs, 'p2p.PID_'+str(self.mapp2p.pid)+
                            '.participant_add_udp', (pIP, msg_id,))

    def participant_add_udp(self, _rpc_caller, caller_id, callee_nip, 
                            callee_netid, pIP, msg_id):
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
                    'called by ' + str(pIP) + ' with msg_id = ' + 
                    str(msg_id))
        ip = self.maproute.nip_to_ip(pIP)
        if not check_ids(ip, msg_id):
            #raise Exception('The message is now expired')
            rpc.UDP_send_reply(_rpc_caller, caller_id, None)
        elif self.maproute.me == callee_nip and \
             self.neigh.netid == callee_netid:
            self.participant_add(pIP, msg_id)
            # Since it is micro, I will reply None
            rpc.UDP_send_reply(_rpc_caller, caller_id, None)

    @microfunc(True)
    def participant_add(self, pIP, msg_id):
        '''Add a participant node to the P2P service

        :param pIP: participant node's Netsukuku IP (nip)
        '''

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
                self.call_participant_add_udp(nr, pIP, msg_id)
                logging.debug('done forwarding participant_add(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))
            except:
                logging.debug('timeout (no problem) forwarding '
                              'participant_add(%s) to %s.' % 
                              (pIP, self.maproute.ip_to_nip(nr.ip)))


    def msg_send(self, sender_nip, hip, msg, msg_id):
        """Routes a packet to `hip'. Do not use this function directly, use
        self.peer() instead

        msg: it is a (func_name, args) pair."""
        
        logging.log(logging.ULTRADEBUG, 'P2P: participant_add_udp '
                    'called by ' + str(sender_nip) + ' with msg_id = ' + 
                    str(msg_id))
        ip = self.maproute.nip_to_ip(sender_nip)
        if not check_ids(ip, msg_id):
            raise Exception('The message is now expired')
        
        logging.log(logging.ULTRADEBUG, 'Someone is asking for P2P '
                                        'service to ' + str(hip))
        H_hip = self.H(hip)
        logging.log(logging.ULTRADEBUG, ' nearest known is ' + str(H_hip))
        if H_hip == self.mapp2p.me:
            # the msg has arrived
            logging.debug('I have been asked a P2P service, as the '
                          'nearest to ' + str(hip) + ' (msg=' + str(msg) +
                          ')')
            return self.msg_exec(sender_nip, msg, msg_id)

        # forward the message until it arrives at destination
        n = self.neigh_get(H_hip)
        if n:
            logging.log(logging.ULTRADEBUG, ' through ' + str(n))
            ret = None
            execstr = 'ret = n.ntkd.p2p.PID_' + str(self.mapp2p.pid) + \
            '.msg_send(sender_nip, hip, msg, msg_id)'
            logging.log(logging.ULTRADEBUG, 'Executing "' + execstr + 
                        '" ...')
            exec(execstr)
            logging.log(logging.ULTRADEBUG, 'Executed "' + execstr + 
                        '". Returning ' + str(ret))
            return ret
        else:
            # Is it possible? Don't we retry?
            logging.warning('I don\'t know to whom I must forward. '
                            'Giving up. Raising exception.')
            logging.warning('This is mapp2p.')
            logging.warning(self.mapp2p.repr_me())
            logging.warning('This is maproute.')
            logging.warning(Map.repr_me(self.maproute))
            raise Exception('Unreachable P2P destination ' + str(H_hip) + 
                            ' from ' + str(self.maproute.me) + '.')

    def call_msg_send_udp(self, neigh, sender_nip, hip, msg, msg_id):
        """Use BcastClient to call msg_send"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'p2p.PID_' + 
                            str(self.mapp2p.pid) + '.msg_send_udp', 
                            (sender_nip, hip, msg, msg_id))

    def msg_send_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid, 
                     sender_nip, hip, msg, msg_id):
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
        ip = self.maproute.nip_to_ip(sender_nip)
        if not check_ids(ip, msg_id):
            #raise Exception('The message is now expired')
            rpc.UDP_send_reply(_rpc_caller, caller_id, ret)
        elif self.maproute.me == callee_nip and \
           self.neigh.netid == callee_netid:
            ret = None
            rpc.UDP_send_keepalive_forever_start(_rpc_caller, caller_id)
            try:
                logging.log(logging.ULTRADEBUG, 'calling msg_send...')
                ret = self.msg_send(sender_nip, hip, msg, msg_id)
                logging.log(logging.ULTRADEBUG, 'returning ' + str(ret))
            except Exception as e:
                ret = ('rmt_error', e.message)
                logging.warning('msg_send_udp: returning exception ' + 
                                str(ret))
            finally:
                rpc.UDP_send_keepalive_forever_stop(caller_id)
            logging.log(logging.ULTRADEBUG, 'calling UDP_send_reply...')
            rpc.UDP_send_reply(_rpc_caller, caller_id, ret)

    def msg_exec(self, sender_nip, msg, msg_id):
        return self.dispatch(CallerInfo(), *msg)

    class RmtPeer(FakeRmt):
        def __init__(self, p2p, hIP=None, key=None, neigh=None):
            self.p2p = p2p
            self.key = key
            self.hIP = hIP
            self.neigh = neigh
            FakeRmt.__init__(self)

        def prepare_rmt(self):
            if self.hIP is None:
                self.hIP = self.p2p.h(self.key)
            if self.hIP is None:
                raise Exception, "'key' does not map to a IP."
            self.H_hip = self.p2p.H(self.hIP)
            
        def peer_is_me(self):
            return self.H_hip == self.p2p.maproute.me

        def peer_get_neigh(self):
            if self.H_hip is None:
                return None
            if self.peer_is_me():
                raise Exception, ("Peer is me. You shouldn't ask for "
                                  "neigh, without checking.")
            return self.p2p.neigh_get(self.H_hip)

        def rmt(self, func_name, *params):
            """Overrides FakeRmt.rmt()"""
            self.prepare_rmt()
            if self.neigh:
                # We are requested to use this one as first hop via UDP.
                logging.log(logging.ULTRADEBUG, 'P2P: Use UDP via ' + 
                            str(self.neigh) + ' to reach peer.')
                return self.p2p.call_msg_send_udp(self.neigh, 
                                                  self.p2p.maproute.me, 
                                                  self.hIP, 
                                                  (func_name, params,),
                                                  updated_id())
            else:
                # Use TCP version.
                logging.log(logging.ULTRADEBUG, 'P2P: Use TCP to reach peer.')
                return self.p2p.msg_send(self.p2p.maproute.me, self.hIP, 
                                         (func_name, params,), updated_id())

    def peer(self, hIP=None, key=None, neigh=None):
        if hIP is None and key is None:
                raise Exception, ("hIP and key are both None. "
                                  "Specify at least one")
        return self.RmtPeer(self, hIP=hIP, key=key, neigh=neigh)


class P2PAll(object):
    """Class of all the registered P2P services"""

    __slots__ = ['ntkd',
                 'radar',
                 'neigh',
                 'maproute',
                 'service',
                 'remotable_funcs',
                 'events',
                 'etp']

    def __init__(self, ntkd, radar, maproute, etp):
        self.ntkd = ntkd

        self.radar = radar
        self.neigh = radar.neigh
        self.maproute = maproute
        self.etp = etp

        self.service = {}

        self.remotable_funcs = [self.pid_getall]
        self.events=Event(['P2P_HOOKED'])
        ###self.etp.events.listen('HOOKED', self.p2p_hook)

    def pid_add(self, pid):
        logging.log(logging.ULTRADEBUG, 'Called P2PAll.pid_add...')
        self.service[pid] = P2P(self.radar, self.maproute, pid)
        return self.service[pid]

    def pid_del(self, pid):
        if pid in self.service:
            del self.service[pid]

    def pid_get(self, pid):
        if pid not in self.service:
            return self.pid_add(pid)
        else:
            return self.service[pid]

    def pid_getall(self):
        return [(s, self.service[s].mapp2p.map_data_pack())
                        for s in self.service]


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
            map_pack = self.pid_get(p2p.pid).mapp2p.map_data_pack()
            p2p.mapp2p.map_data_merge(map_pack)
        self.service[p2p.pid] = p2p

    #  TODO  DELETED. Ok?
    #def participant_add(self, pid, pIP):
    #    self.pid_get(pid).participant_add(pIP)

    @microfunc()
    def p2p_hook(self, *args):
        """P2P hooking procedure

        It gets the P2P maps from our nearest neighbour"""

        logging.log(logging.ULTRADEBUG, 'P2P hooking: started')
        logging.log(logging.ULTRADEBUG, 'P2P hooking: My actual list of '
                    'services is: ' + str(self.log_services()))
        ## Find our nearest neighbour
        neighs_in_net = self.neigh.neigh_list(in_my_network=True)
        while True:
            minlvl = self.maproute.levels
            minnr = None
            for nr in neighs_in_net:
                lvl = self.maproute.nip_cmp(self.maproute.me, nr.nip)
                if lvl < minlvl:
                    minlvl = lvl
                    minnr  = nr
            ##

            if minnr is None:
                # nothing to do
                logging.log(logging.ULTRADEBUG, 'P2P hooking: No neighbours '
                            'to ask for the list of services.')
                break

            logging.log(logging.ULTRADEBUG, 'P2P hooking: I will ask for the '
                        'list of services to ' + str(minnr))
            try:
                nrmaps_pack = minnr.ntkd.p2p.pid_getall()
            except:
                logging.warning('P2P hooking: Asking to ' + str(minnr) + 
                                ' failed.')
                neighs_in_net.remove(minnr)
                continue
            logging.log(logging.ULTRADEBUG, 'P2P hooking: ' + str(minnr) + 
                        ' answers ' + str(nrmaps_pack))
            for (pid, map_pack) in nrmaps_pack:
                self.pid_get(pid).mapp2p.map_data_merge(map_pack)

            for s in self.service:
                    if self.service[s].participant:
                            self.service[s].participate()
            logging.log(logging.ULTRADEBUG, 'P2P hooking: My final list of '
                        'services is: ' + str(self.log_services()))
            break

        self.events.send('P2P_HOOKED', ())

    def __getattr__(self, str):

        if str[:4] == "PID_":
            return self.pid_get(int(str[4:]))
        raise AttributeError

    def log_services(self):
        return [(s, self.service[s].mapp2p.repr_me())
                        for s in self.service]

