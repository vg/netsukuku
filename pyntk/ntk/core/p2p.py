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

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime
from ntk.lib.log import logger as logging
from ntk.lib.event import Event
from ntk.lib.rpc   import FakeRmt, RPCDispatcher, CallerInfo
from ntk.lib.micro import microfunc, Channel
from ntk.lib.rencode import serializable
from ntk.core.map import Map


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
        # lvl and id are not used (as for now) at the time of de-serialization. So
        # use the value that will produce the smaller output with rencode.dumps.
        # TODO test what this value is... perhaps None is better than 0 ?
        return (0, 0, self.participant)

serializable.register(ParticipantNode)

class MapP2P(Map):
    """Map of the participant nodes"""

    def __init__(self, levels, gsize, me, pid):
        """levels, gsize, me: the same of Map

        pid: P2P id of the service associated to this map
        """

        Map.__init__(self, levels, gsize, ParticipantNode, me)

        self.pid = pid

    def participate(self):
        """Set self.me to be a participant node."""

        for l in xrange(self.levels):
            node = self.node_get(l, self.me[l])
            was_free = node.is_free()
            node.participant = True
            if was_free and not node.is_free():
                self.node_add(l, self.me[l])

    @microfunc()
    def me_changed(self, old_me, new_me):
        '''Changes self.me

        :param old_me: my old nip (not used in MapP2P)
        :param new_me: new nip
        '''
        Map.me_change(self, new_me)

    @microfunc(True)
    def node_del(self, lvl, id):
        Map.node_del(self, lvl, id)

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

        self.maproute.events.listen('ME_CHANGED', self.mapp2p.me_changed)
        self.maproute.events.listen('NODE_DELETED', self.mapp2p.node_del)

        # are we a participant?
        self.participant = False

        self.remotable_funcs = [self.participant_add, self.msg_send, self.msg_send_udp, self.reply_msg_send_udp]

        RPCDispatcher.__init__(self, root_instance=self)

    def h(self, key):
        """This is the function h:KEY-->IP.

        You should override it with your own mapping function.
        """
        return key

    def H(self, IP):
        """This is the function that maps each IP to an existent hash node IP
           If there are no participants, None is returned"""
        mp = self.mapp2p
        hIP = [None] * mp.levels
        for l in reversed(xrange(mp.levels)):
                for id in xrange(mp.gsize):
                        for sign in [-1,1]:
                                hid=(IP[l] + id * sign) % mp.gsize
                                if mp.node_get(l, hid).participant:
                                        hIP[l] = hid
                                        break
                        if hIP[l]:
                                break
                if hIP[l] is None:
                        return None

                if hIP[l] != mp.me[l]:
                        # we can stop here
                        break

        return hIP

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

    def participate(self):
        """Let's become a participant node"""
        self.participant = True
        self.mapp2p.participate()
        current_nr_list = self.neigh.neigh_list()

        # TODO handle the case where one of neighbours does not reply (raises an error)
        for nr in current_nr_list:
            try:
                logging.debug('calling participant_add(myself) to %s.' % self.maproute.ip_to_nip(nr.ip))
                stringexec = "nr.ntkd.p2p.PID_"+str(self.mapp2p.pid)+".participant_add(self.maproute.me)"
                logging.debug(stringexec)
                exec(stringexec)
                logging.debug('done calling participant_add(myself) to %s.' % self.maproute.ip_to_nip(nr.ip))
            except:
                logging.debug('timeout (no problem) calling participant_add(myself) to %s.' % self.maproute.ip_to_nip(nr.ip))

    @microfunc(True)
    def participant_add(self, pIP):
        '''Add a participant node to the P2P service

        :param pIP: participant node's Netsukuku IP (nip)
        '''

        continue_to_forward = False
        current_nr_list = self.neigh.neigh_list()
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
                logging.debug('registering participant (%s, %s) to service %s.' % (l, pIP[l], mp.pid))
                mp.node_get(l, pIP[l]).participant = True
                mp.node_add(l, pIP[l])
                continue_to_forward = True

        if not continue_to_forward:
            return

        # continue to advertise the new participant

        # TODO handle the case where one of neighbours does not reply (raises an error)
        # TODO do we have to skip the one who sent to us? It is not needed cause it won't forward anyway.

        for nr in current_nr_list:
            try:
                logging.debug('forwarding participant_add(%s) to %s service %s.' % (pIP, self.maproute.ip_to_nip(nr.ip), mp.pid))
                stringexec = "nr.ntkd.p2p.PID_"+str(mp.pid)+".participant_add(pIP)"
                logging.debug(stringexec)
                exec(stringexec)
                logging.debug('done forwarding participant_add(%s) to %s.' % (pIP, self.maproute.ip_to_nip(nr.ip)))
            except:
                logging.debug('timeout (no problem) forwarding participant_add(%s) to %s.' % (pIP, self.maproute.ip_to_nip(nr.ip)))


    def msg_send(self, sender_nip, hip, msg, use_udp_nip=None):
        """Routes a packet to `hip'. Do not use this function directly, use
        self.peer() instead

        msg: it is a (func_name, args) pair."""

        if self.maproute.me == sender_nip:
            logging.log(logging.ULTRADEBUG, 'I am asking for P2P service to ' + str(hip))
        else:
            logging.log(logging.ULTRADEBUG, str(sender_nip) + ' is asking for P2P service to ' + str(hip) + '. I will forward...')
        if use_udp_nip:
            logging.log(logging.ULTRADEBUG, ' using UDP through ' + str(use_udp_nip))
            return self.call_msg_send_udp(use_udp_nip, sender_nip, hip, msg)
        else:
            logging.log(logging.ULTRADEBUG, ' using TCP')
            H_hip = self.H(hip)
            logging.log(logging.ULTRADEBUG, ' nearest known is ' + str(H_hip))
            if H_hip == self.mapp2p.me:
                # the msg has arrived
                logging.debug('I have been asked a P2P service, as the nearest to ' + str(hip) + ' (sender=' + str(sender_nip) + ', msg=' + str(msg) + ')')
                return self.msg_exec(sender_nip, msg)

            # forward the message until it arrives at destination
            n = self.neigh_get(H_hip)
            if n:
                logging.log(logging.ULTRADEBUG, ' forwarding to ' + str(self.maproute.ip_to_nip(n.ip)))
                ret = None
                stringexec = "ret = n.ntkd.p2p.PID_"+str(self.mapp2p.pid) + \
                     ".msg_send(sender_nip, hip, msg)"
                logging.log(logging.ULTRADEBUG, 'Calling ' + stringexec)
                exec(stringexec)
                logging.log(logging.ULTRADEBUG, 'Calling ' + stringexec + '  done. Got reply. Returning ' + str(ret))
                return ret
            else:
                # Is it possible? Don't we retry?
                logging.warning('I don\'t know to whom I must forward. Giving up. Returning None.')
                logging.warning('This is mapp2p.')
                def repr_node_mapp2p(node):
                    if node.participant: return 'X'
                    return ' '
                logging.warning(self.mapp2p.repr_me(repr_node_mapp2p))
                logging.warning('This is maproute.')
                logging.warning(Map.repr_me(self.maproute))
                return None

    def call_msg_send_udp(self, nip, sender_nip, hip, msg):
        """Use BcastClient to call msg_send"""
        devs = self.radar.broadcast.devs
        try:
            devs = [self.neigh.ip_to_neigh(self.maproute.nip_to_ip(nip)).bestdev[0]]
            logging.log(logging.ULTRADEBUG, 'I\'ll create a BcastClient with NIC = ' + devs[0])
        except:
            logging.log(logging.ULTRADEBUG, 'I\'ll create a BcastClient with any handled NIC')
        return rpc.UDP_call(nip, devs, 'p2p.PID_'+str(self.mapp2p.pid)+'.msg_send_udp', (sender_nip, hip, msg))

    def msg_send_udp(self, _rpc_caller, caller_id, nip_callee, sender_nip, hip, msg):
        """Returns the result of msg_send to remote caller.
           caller_id is the random value generated by the caller for this call.
            It is replied back to the LAN for the caller to recognize a reply destinated to it.
           nip_callee is the NIP of the callee.
            It is used by the callee to recognize a request destinated to it.
           """
        if self.maproute.me == nip_callee:
            ret = self.msg_send(sender_nip, hip, msg)
            rpc.UDP_send_reply(_rpc_caller, caller_id, 'p2p.PID_'+str(self.mapp2p.pid)+'.reply_msg_send_udp', ret)

    def reply_msg_send_udp(self, _rpc_caller, caller_id, ret):
        """Receives reply from msg_send_udp."""
        rpc.UDP_got_reply(_rpc_caller, caller_id, ret)

    def msg_exec(self, sender_nip, msg):
        return self.dispatch(CallerInfo(), *msg)

    class RmtPeer(FakeRmt):
        def __init__(self, p2p, hIP=None, key=None, use_udp_nip=None):
            self.p2p = p2p
            self.key = key
            self.hIP = hIP
            self.use_udp_nip = use_udp_nip
            FakeRmt.__init__(self)

        def rmt(self, func_name, *params):
            """Overrides FakeRmt.rmt()"""
            if self.hIP is None:
                    self.hIP = self.p2p.h(self.key)
            return self.p2p.msg_send(self.p2p.maproute.me, self.hIP, (func_name, params), use_udp_nip=self.use_udp_nip)


    def peer(self, hIP=None, key=None, use_udp_nip=None):
        if hIP is None and key is None:
                raise Exception, "hIP and key are both None. Specify at least one"
        return self.RmtPeer(self, hIP=hIP, key=key, use_udp_nip=use_udp_nip)


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
        self.etp.events.listen('COMPLETE_HOOK', self.p2p_hook)

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
        def fmake_participant(node):
            node.participant = True
        return [(s, self.service[s].mapp2p.map_data_pack(fmake_participant))
                        for s in self.service]


    def p2p_register(self, p2p):
        """Used to add for the first time a P2P instance of a module in the
           P2PAll dictionary."""

        logging.log(logging.ULTRADEBUG, 'Called P2PAll.p2p_register for ' + str(p2p.pid) + '...')
        # It's possible that the stub P2P instance `self.pid_get(p2p.pid)'
        # created by pid_add() has an update map of participants, which has
        # been accumulated during the time. Copy this map in the `p2p'
        # instance to be sure.
        def fmake_participant(node):
            node.participant = True
        if p2p.pid in self.service:
            logging.log(logging.ULTRADEBUG, 'Called P2PAll.p2p_register for ' + str(p2p.pid) + '... cloning...')
            map_pack = self.pid_get(p2p.pid).mapp2p.map_data_pack(fmake_participant)
            p2p.mapp2p.map_data_merge(map_pack)
        self.service[p2p.pid] = p2p

    #  TODO  DELETED. Ok?
    #def participant_add(self, pid, pIP):
    #    self.pid_get(pid).participant_add(pIP)

    @microfunc()
    def p2p_hook(self):
        """P2P hooking procedure

        It gets the P2P maps from our nearest neighbour"""

        if self.ntkd.neighbour.netid == -1:
            # I'm not ready to interact.
            return

        logging.log(logging.ULTRADEBUG, 'P2P hooking: started')
        logging.log(logging.ULTRADEBUG, 'P2P hooking: My actual list of services is: ' + str(self.log_services()))
        ## Find our nearest neighbour
        minlvl = self.maproute.levels
        minnr = None
        for nr in self.neigh.neigh_list():
            lvl = self.maproute.nip_cmp(self.maproute.me,
                                        self.maproute.ip_to_nip(nr.ip))
            if lvl < minlvl:
                minlvl = lvl
                minnr  = nr
        ##

        if minnr is None:
                # nothing to do
                logging.log(logging.ULTRADEBUG, 'P2P hooking: No neighbours to ask for the list of services.')
                return


        logging.log(logging.ULTRADEBUG, 'P2P hooking: I will ask for the list of services to ' + str(self.maproute.ip_to_nip(minnr.ip)))
        nrmaps_pack = minnr.ntkd.p2p.pid_getall()
        for (pid, map_pack) in nrmaps_pack:
            self.pid_get(pid).mapp2p.map_data_merge(map_pack)

        for s in self.service:
                if self.service[s].participant:
                        self.service[s].participate()
        logging.log(logging.ULTRADEBUG, 'P2P hooking: My final list of services is: ' + str(self.log_services()))


        self.events.send('P2P_HOOKED', ())

    def __getattr__(self, str):

        if str[:4] == "PID_":
            return self.pid_get(int(str[4:]))
        raise AttributeError

    def log_services(self):
        def repr_node_mapp2p(node):
            if node.participant: return 'X'
            return ' '
        return [(s, self.service[s].mapp2p.repr_me(repr_node_mapp2p))
                        for s in self.service]

