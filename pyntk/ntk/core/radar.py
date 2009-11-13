# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2007 Alberto Santini <alberto@unix-monk.com>
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
# The radar sends in broadcast a bouquet of MAX_BOUQUET packets and waits
# for the reply of the alive nodes. It then recollects the replies and builds
# a small statistic.
# By watching the previous statistics, it can deduces if a change occurred or
# not. If it is, one of the following events is fired:
#        'NEIGH_NEW', 'NEIGH_DELETED', 'NEIGH_REM_CHGED'
# In this way, the other modules of pyntk will be noticed.
#
# A radar is fired periodically by Radar.run(), which is started as a
# microthread.
#
# TODO: the current statistic is based on the RTT (Round Trip Time)
# of the packets. However, more refined way can be used and shall be
# implemented: first of all consider the number of bouquet packets lost, then
# see NTK_RFC 0002  http://lab.dyne.org/Ntk_bandwidth_measurement
#
##

import time

from random import randint

import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime

from ntk.config import settings
from ntk.core.route import DeadRem, Rtt
from ntk.lib.event import Event
from ntk.lib.log import get_stackframes, log_exception_stacktrace
from ntk.lib.log import logger as logging
from ntk.lib.micro import micro, micro_block, microfunc, Channel
from ntk.network.inet import ip_to_str, str_to_ip
from ntk.core.status import ZombieException


class Neigh(object):
    """This class simply represent a neighbour"""

    __slots__ = ['devs', 'bestdev', 'ip', 'rem', 'ntkd_func', 'ntkd', 'netid', 'macs']

    def func_none(*args):
        return None

    def __init__(self, bestdev, devs, ip, netid, macs,
                 ntkd_func=func_none):
        """
        ip: neighbour's ip;
        netid: network id of the node
              ip + netid = unique key.
        devs: a dict which maps a device to the average rtt
        bestdev: a pair (d, avg_rtt), where devs[d] is the best element of
                devs.
        macs: a sequence of string: the various MACs of the neighbour

        ntkd_func: a function such that:
            ntkd_func(ip, netid) returns neighbour's ntk remote instance.
        """

        self.devs = devs
        self.bestdev = bestdev
        self.ip = ip
        self.netid = netid
        self.macs = macs

        if self.bestdev:
            # TODO(low): support the other metrics
            self.rem = Rtt(self.bestdev[1])
        else:
            self.rem = DeadRem() # The neighbour is dead
        self.ntkd_func = ntkd_func

    def _get_ntkd(self):
        return self.ntkd_func(self.ip, self.netid)

    ntkd = property(_get_ntkd)

    def __cmp__(self, b):
        stacktrace = get_stackframes(back=1)
        logging.warning('Neigh.__cmp__ called at ' + stacktrace)
        return (self.ip > b.ip) - (self.ip < b.ip)

    def __repr__(self):
        if not self.ip: return object.__repr__(self)
        if not self.rem: return '<Neighbour(%s in %s): No rem>' % \
                        (ip_to_str(self.ip), self.netid)
        return '<Neighbour(%s in %s):%s>' % (ip_to_str(self.ip), 
                                             self.netid, self.rem)

    def values(self):
        '''Returns a dict rappresentation of the neighbour

        self.ntkd is excluded: it's a TCPClient so is useless to perform
                               checks
        '''
        v = [(name, getattr(self, name)) for name in self.__slots__
             if name != 'ntkd']
        return dict(v)

class Neighbour(object):
    """ This class manages all neighbours """

    __slots__ = ['ntkd_status',
                 'time_tick_serializer',
                 'radar',
                 'maproute',
                 'max_neigh',
                 'rtt_variation_threshold',
                 'ip_netid_table',
                 'ntk_client',
                 'events',
                 'remotable_funcs',
                 'xtime',
                 'netid',
                 'increment_wait_time',
                 'number_of_scan_before_deleting',
                 'missing_neighbour_keys',
                 'channels']

    def __init__(self, ntkd_status, time_tick_serializer, radar, maproute, max_neigh=settings.MAX_NEIGH, xtimemod=xtime):
        """  max_neigh: maximum number of neighbours we can have """

        self.ntkd_status = ntkd_status
        self.time_tick_serializer = time_tick_serializer
        self.maproute = maproute
        self.radar = radar
        
        self.max_neigh = max_neigh
        # variation on neighbours' rtt greater than this will be notified
        # TODO changed to do less variations in netkit environment
        #self.rtt_variation_threshold = 0.1
        self.rtt_variation_threshold = 0.9
        # ip_netid_table
        # This is a dict whose key is a pair (ip, netid), that is the unique
        # identifier of a neighbour node. The same ip could be assigned to two
        # or more neighbours if they are from different networks.
        # The values of this dict are instances of Neigh.
        self.ip_netid_table = {}
        # ntk_client
        # This is a dict mapping an ip to a TCPClient instance. Only 
        # neighbours that are in our same network have a TCPClient, so 
        # netid is not in the key.
        self.ntk_client = {}
        # the events we raise
        self.events = Event(['NEIGH_NEW', 'NEIGH_DELETED', 'NEIGH_REM_CHGED',
                      'COLLIDING_NEIGH_NEW', 'COLLIDING_NEIGH_DELETED',
                      'COLLIDING_NEIGH_REM_CHGED', 'TIME_TICK'])
        # time module
        self.xtime = xtimemod
        # channels for the methods to synchronize routes in the kernel table
        self.channels = {}

        # Our netid. It's a random id used to detect network collisions.
        self.netid = -1

        # To be certain, before deleting a neighbour, check a few times with
        # a greater delay.
        self.increment_wait_time = 1000
        self.number_of_scan_before_deleting = 1
        # This is a dict. The key is the neigh id, the value is missing_scans.
        # e.g. {2:4} means neighbour 2 has not replied for 4 consecutive
        # scans.
        self.missing_neighbour_keys = {}

        self.remotable_funcs = [self.readvertise_udp,
                                self.ip_netid_change,
                                self.ip_netid_change_udp,
                                self.ip_netid_change_broadcast_udp]
        self.monitor_neighbours()

        self.events.listen('TIME_TICK', self.readvertise_local)
        #self.etp.events.listen('TIME_TICK', self.readvertise_local)
        # This is placed in module qspn. TODO refactor.

    @microfunc(True)
    def monitor_neighbours(self):
        while True:
            xtime.swait(100)
            known_neighs = '{'
            for ip, netid in self.ip_netid_table:
                nip = self.maproute.ip_to_nip(ip)
                known_neighs += '(' + str(nip) + ',' + str(netid) + ')  '
            known_neighs += '}'
            logging.log(logging.ULTRADEBUG, 'monitor_neighbours: '
                        'DELETETHISLOG - Known Neighbours: ' + known_neighs)

    @microfunc(True)
    def readvertise_local(self, old_node_nb, cur_node_nb):
        '''Detects and handle the cases of Communicating Vessels and
           Gnode Splitting.'''

        # Note: this function handles the event TIME_TICK locally

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, better not to emit any signal.
            stacktrace = get_stackframes()
            logging.debug('Neighbour.readvertise: I\'m hooking, better not to emit any signal, while at ' + stacktrace)
            return

        logging.debug('Neighbour.readvertise_local microfunc started.')
        ## Chech if the size of network has changed.
        def add(a,b):return a+b
        if reduce(add, old_node_nb) == reduce(add, cur_node_nb):
            # If not, exit
            logging.debug('Neighbour.readvertise does NOT need to be executed.')
            return

        # Get only the neighbours OUT of my network.
        for neigh in self.neigh_list(out_of_my_network=True):
            logging.debug('Neighbour.readvertise_local will send to ' + str(neigh))
            key = (neigh.ip, neigh.netid)
            self.add(key)
            # This call is a microfunc and won't schedule right now.
            # So we should not have problems with the list obtained with neigh_list.
            self.call_readvertise_udp_tasklet(neigh)

        logging.debug('Neighbour.readvertise_local microfunc done. exiting.')

    @microfunc(True)
    def call_readvertise_udp_tasklet(self, neigh):
        try:
            self.call_readvertise_udp(neigh)
        except Exception, e:
            logging.debug('Neighbour.readvertise_local: Exception while asking to ' \
              + str(neigh) + ' to readvertise: ' + str(e))

    @microfunc(True)
    def readvertise(self):
        '''Detects and handle the cases of Communicating Vessels and
           Gnode Splitting.'''

        # Note: We are called directly by a neighbour.

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, better not to emit any signal.
            stacktrace = get_stackframes()
            logging.debug('Neighbour.readvertise: I\'m hooking, better not to emit any signal, while at ' + stacktrace)
            return

        logging.debug('Neighbour.readvertise microfunc started. Called remotely.')

        # Get only the neighbours OUT of my network.
        for neigh in self.neigh_list(out_of_my_network=True):
            logging.debug('Neighbour.readvertise will send to ' + str(neigh))
            key = (neigh.ip, neigh.netid)
            self.add(key)

        logging.debug('Neighbour.readvertise microfunc done. exiting.')

    def call_readvertise_udp(self, neigh):
        """Use BcastClient to call readvertise"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'neighbour.readvertise_udp')

    def readvertise_udp(self, _rpc_caller, caller_id, callee_nip, callee_netid):
        """Returns the result of readvertise to remote caller.
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
           self.netid == callee_netid:
            self.readvertise()
            # Since it is micro, I will reply None
            # An error here is ignorable.
            try:
                rpc.UDP_send_reply(_rpc_caller, caller_id, None)
            except:
                logging.debug("readvertise_udp: Exception while replying. Ignore.")

    def neigh_list(self, in_my_network=False, out_of_my_network=False,
                         in_this_netid=None, out_of_this_netid=None):
        """ Returns the list of neighbours.
            If in_my_network == True, then returns only nodes
              that are in my network.
            Else, if out_of_my_network == True, then returns only nodes
              that are NOT in my network.
            Else, if in_this_netid is not None, then returns only nodes
              that are in the network with this netid.
            Else, if out_of_this_netid is not None, then returns only nodes
              that are NOT in the network with this netid.
            Else all the neighbours are returned.
            netid == -1 is a special case. It is not in any network.
            So, in_this_netid=-1 is a non-sense, but will simply return
            a void list.
            On the other hand, out_of_this_netid=-1 will return all the
            neighbours.
        """
        # ATTENTION: this method MUST NOT pass schedule while
        # gathering data from the structures.

        def in_my_network_requirement(netid):
            if netid == -1:
                return False
            return netid == self.netid
        def out_of_my_network_requirement(netid):
            if netid == -1:
                return True
            return netid != self.netid
        def in_this_netid_requirement(netid):
            if netid == -1:
                return False
            return netid == in_this_netid
        def out_of_this_netid_requirement(netid):
            if netid == -1:
                return True
            return netid != in_this_netid
        def no_requirements(netid):
            return True

        requirement = no_requirements
        if in_my_network:
            requirement = in_my_network_requirement
        elif out_of_my_network:
            requirement = out_of_my_network_requirement
        elif in_this_netid is not None:
            requirement = in_this_netid_requirement
        elif out_of_this_netid is not None:
            requirement = out_of_this_netid_requirement

        nlist = []
        for key, val in self.ip_netid_table.items():
            ip, netid = key
            if not requirement(netid):
                # this one is not wanted
                continue
            logging.log(logging.ULTRADEBUG, 'neigh_list: returns Neigh for '
                        'nip ' + str(self.maproute.ip_to_nip(ip)) + 
                        ', netid ' + str(netid))
            nlist.append(val)
        return nlist

    def memorize(self, val):
        """ key = (val.ip, val.netid)
            key should not be already in translation table.
            Inserts this neighbour in our data structures. 
            Returns the assigned id.
            If there is no more room, sends an exception.
        """
        # ATTENTION: this method MUST NOT pass schedule until the end.
        key = (val.ip, val.netid)
        if key in self.ip_netid_table:
            raise Exception('Key was already present.')
        # Check for Max Neigh Exceeded
        if len(self.ip_netid_table) >= self.max_neigh:
            raise Exception('Max Neigh Exceeded')
        ip, netid = key
        self.ip_netid_table[key] = val
        if self.netid == netid:
            # It's in my network
            self.ntk_client[ip] = rpc.TCPClient(ip_to_str(ip))

    def unmemorize(self, key):
        """ key: pair ip, netid
            key should be in translation table.
            Removes this neighbour in our data structures.
            Returns old id.
        """
        # ATTENTION: this method MUST NOT pass schedule until the end.
        if key not in self.ip_netid_table:
            raise Exception('Key was not present.')
        ip, netid = key
        del self.ip_netid_table[key]
        if self.netid == netid:
            # It was in my network
            if ip in self.ntk_client:
                del self.ntk_client[ip]

    #############################################################
    ######## Synchronization gateway <-> nodes
    # Abstract:
    #
    # When Neighbour notice a new neighbour (gateway) it calls announce_gw.
    # Then, Neighbour emits event NEIGH_NEW which in turn does other
    # things...
    # When KrnlRoute actually has added the route towards the gateway,
    # it calls announce_gw_added.
    #
    # When Neighbour notice a dead link towards a neighbour it calls
    # announce_gw_removing.
    # Then, Neighbour emits event NEIGH_DELETED which in turn does other
    # things...
    # When MapRoute.routeneigh_del has finished deleting all the
    # routes passing through it, it calls announce_gw_removable.
    #
    # Before actually adding a route through a gateway we must
    # call waitfor_gw_added.
    # Before actually removing the route towards a gateway we
    # must call waitfor_gw_removable.
    #
    # This way we ensure to add a gateway before adding routes
    # through it, and to remove routes through a gateway before
    # removing the gateway itself.

    def announce_gw(self, key):
        # This place should be void and nobody should be receiving
        # in it, but just to be sure:
        if key in self.channels:
            channel = self.channels[key]
            channel.bcast_send('')
            micro_block()
        # Now the real announce.
        self.channels[key] = Channel(prefer_sender=True)

    def waitfor_gw_added(self, key):
        if key in self.channels:
            channel = self.channels[key]
            channel.recv()

    def announce_gw_added(self, key):
        if key in self.channels:
            channel = self.channels[key]
            channel.bcast_send('')
            micro_block()
            del self.channels[key]

    def announce_gw_removing(self, key):
        # This place should be void and nobody should be receiving
        # in it, but just to be sure:
        if key in self.channels:
            channel = self.channels[key]
            channel.bcast_send('')
            micro_block()
        # Now the real announce.
        self.channels[key] = Channel(prefer_sender=True)

    def waitfor_gw_removable(self, key):
        if key in self.channels:
            channel = self.channels[key]
            channel.recv()

    def announce_gw_removable(self, key):
        if key in self.channels:
            channel = self.channels[key]
            channel.bcast_send('')
            micro_block()
            del self.channels[key]

    ##
    #############################################################

    def get_ntk_client(self, ip, netid):
        """ip: neighbour's ip;
           netid: neighbour's netid."""
        if netid == self.netid:
            if ip in self.ntk_client:
                return self.ntk_client[ip]
            else:
                logging.warning('Neighbour.get_ntk_client: '
                            'not present for ip ' + str(ip) + ', netid ' + 
                            str(netid))
                return None
        else:
            return None

    def key_to_neigh(self, key):
        """ key: neighbour's key, that is the pair ip, netid
            return a Neigh object from its ip and netid
        """
        if key not in self.ip_netid_table:
            return None
        else:
            return self.ip_netid_table[key]

    def _truncate(self, ip_netid_table):
        """ip_netid_table: an {(ip, netid) => Neigh};
        we want the best (with the lowest rtt) max_neigh nodes only to
        remain in the table
        """

        # auxiliary function, to take rtt from {(ip, netid) => Neigh}
        def interesting(x):
            return x[1].bestdev[1]

        # the new table, without truncated rows
        ip_netid_table_trunc = {}

        counter = 0

        # we're cycling through ip_netid_table, ordered by rtt
        for key, val in sorted(ip_netid_table.items(), reverse=False, 
                               key=interesting):
            # if we haven't still reached max_neigh entries in the 
            # new ip_netid_table
            if counter < self.max_neigh:
                # add the current row into ip_netid_table
                ip_netid_table_trunc[key] = val
                counter += 1
            else:
                break

        # return the new ip_netid_table
        return ip_netid_table_trunc

    def store_add_neigh(self, *args):
        self.time_tick_serializer(self.serialized_store_add_neigh, args)

    def serialized_store_add_neigh(self, key, val):

        ## evaluate current node_nb.
        old_node_nb = self.maproute.node_nb[:]

        # Check that it did NOT exist; otherwise exit.
        if key in self.ip_netid_table:
            return
        # ATTENTION: this method MUST NOT pass schedule while it updates
        #  the data structures, until the call to method "memorize".
        self.memorize(val)
        # Then we send a signal (event) to notify the change. The main handler
        # of this event is the method etp_new_link of class Etp (see New
        # Link Rule of QSPN). This method must not be microfunc, in order for
        # the serialization of "time_tick" methods to work.
        self.add(key)

        ## evaluate current node_nb.
        cur_node_nb = self.maproute.node_nb[:]

        self.events.send('TIME_TICK', (old_node_nb, cur_node_nb))

    def store_delete_neigh(self, *args):
        self.time_tick_serializer(self.serialized_store_delete_neigh, args)

    def serialized_store_delete_neigh(self, key):

        ## evaluate current node_nb.
        old_node_nb = self.maproute.node_nb[:]

        # Check that it did exist; otherwise exit.
        if key not in self.ip_netid_table:
            return
        before_dead_link = {}
        current_nr_list = self.neigh_list(in_my_network=True)
        # Takes note of the current situation, the bits of information that
        # we'll need after this update.
        # ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    # A computes rem(Bestᵗ (A → v))
                    old_best = routes_to_v.best_route()
                    before_dead_link[(lvl, dst)] = (old_best.rem, {})
                    # ∀w ∈ A*
                    for w in current_nr_list:
                        w_lvl_id = self.maproute.routeneigh_get(w)
                        # A checks wether "w ∈ Bestᵗ (A → v)"
                        before_dead_link[(lvl, dst)][1][w_lvl_id] = old_best.contains(w_lvl_id)
        # ATTENTION: this method MUST NOT pass schedule while it updates
        #  the data structures, until the call to method "unmemorize".
        ip, netid = key
        old_val = self.ip_netid_table[key]
        old_ntk_client = self.get_ntk_client(ip, netid)
        self.unmemorize(key)
        # Then we send a signal (event) to notify the change. The main handler
        # of this event is the method etp_dead_link of class Etp (see Changed
        # Link Rule of QSPN). This method must not be microfunc, in order for
        # the serialization of "time_tick" methods to work.
        self.delete(key, old_val, before_dead_link)
        if old_ntk_client is not None:
            if old_ntk_client.connected:
                old_ntk_client.close()

        ## evaluate current node_nb.
        cur_node_nb = self.maproute.node_nb[:]

        self.events.send('TIME_TICK', (old_node_nb, cur_node_nb))

    def store_changed_neigh(self, *args):
        self.time_tick_serializer(self.serialized_store_changed_neigh, args)

    def serialized_store_changed_neigh(self, key, val):

        ## evaluate current node_nb.
        old_node_nb = self.maproute.node_nb[:]

        # Check that it did exist; otherwise exit.
        if key not in self.ip_netid_table:
            return
        before_changed_link = {}
        current_nr_list = self.neigh_list(in_my_network=True)
        # Takes note of the current situation, the bits of information that
        # we'll need after this update.
        # ∀v ∈ V
        for lvl in xrange(self.maproute.levels):
            xtime.sleep_during_hard_work(10)
            for dst in xrange(self.maproute.gsize):
                routes_to_v = self.maproute.node_get(lvl, dst)
                if not routes_to_v.is_empty():
                    xtime.sleep_during_hard_work(0)
                    # A computes rem(Bestᵗ (A → v))
                    old_best = routes_to_v.best_route()
                    before_changed_link[(lvl, dst)] = (old_best.rem, {})
                    # ∀w ∈ A*
                    for w in current_nr_list:
                        w_lvl_id = self.maproute.routeneigh_get(w)
                        # A checks wether "w ∈ Bestᵗ (A → v)"
                        before_changed_link[(lvl, dst)][1][w_lvl_id] = old_best.contains(w_lvl_id)
        # ATTENTION: this method MUST NOT pass schedule while it updates
        #  the data structures.
        old_val = self.ip_netid_table[key]
        old_rtt = old_val.rem
        old_val.devs = val.devs
        old_val.bestdev = val.bestdev
        old_val.rem = Rtt(old_val.bestdev[1])
        # Then we send a signal (event) to notify the change. The main handler
        # of this event is the method etp_dead_link of class Etp (see Changed
        # Link Rule of QSPN). This method must not be microfunc, in order for
        # the serialization of "time_tick" methods to work.
        self.rem_change(key, old_rtt, before_changed_link)

        ## evaluate current node_nb.
        cur_node_nb = self.maproute.node_nb[:]

        self.events.send('TIME_TICK', (old_node_nb, cur_node_nb))

    def store(self, new_ip_netid_table):
        """Detects changes in our neighbourhood. Updates internal data structures
        and notify about the changes.

        new_ip_netid_table: actual version of ip_netid_table, as seen by the radar.
        """

        # The class Radar, on each scan, passes to this method a 
        # dict {key => Neigh}.
        # Neigh is a new complete instance. For new neighbours, it will be
        # added to Neighbours' internal data structures. For old neighbours
        # it will be discarded after updating values in the instance already
        # present in Neighbours' internal data structures.

        # Using _truncate, we remove the worst ones, if they exceed max_neigh.
        new_ip_netid_table = self._truncate(new_ip_netid_table)
        
        # Then, compare the dict against the data structures and summarize
        # the changes (e.g. new Neigh, new Neigh, changed Neigh, ...)
        to_be_deleted = [] # key
        to_be_added = [] # (key, Neigh)
        to_be_changed = [] # (key, Neigh)

        ### remove from missing_neighbour_keys the detected neighbours
        for key in new_ip_netid_table:
            if key in self.missing_neighbour_keys:
                del self.missing_neighbour_keys[key]

        ### We'll remove nodes that were 
        ### present and now are missing, but only after <n> radar scans.
        ### Note: if we have reached the max neigh, we must delete all missing
        ### neighbours, never minding for how many scans they were missing.
        for key in self.ip_netid_table.keys():
            if not key in new_ip_netid_table:
                # It is a missing neigh. For how many scan is it missing?
                if key in self.missing_neighbour_keys:
                    times = self.missing_neighbour_keys[key] + 1
                else:
                    times = 1
                self.missing_neighbour_keys[key] = times
                if self.max_neigh == len(new_ip_netid_table) or \
                   times > self.number_of_scan_before_deleting:
                    # now, we assume it is really dead.
                    del self.missing_neighbour_keys[key]
                    to_be_deleted.append(key)

        ### now, we cycle through the new ip_netid_table
        ### looking for nodes that aren't in the old one.
        for key, val in new_ip_netid_table.items():
            if not key in self.ip_netid_table:
                # It is a new neigh.
                to_be_added.append((key, val))

        ### now we cycle through the new ip_netid_table
        ### looking for nodes whose rtt has sensibly changed
        for key, val_new in new_ip_netid_table.items():
            ip, netid = key
            # if the node is not new
            if key in self.ip_netid_table:
                val = self.ip_netid_table[key]
                # check if its rtt has changed more than rtt_variation
                new_rtt = val_new.bestdev[1]
                old_rtt = val.bestdev[1]
                # using the following algorithm, we accomplish this:
                #  e.g. rtt_variation_threshold = .5, we want to be warned 
                #       when rtt become more than double of previous 
                #       or less than half of previous.
                #  from 200 to 400+ we get warned, 
                #      because (400 - 200) / 400 = 0.5
                #  from 400 to 200- we get warned, 
                #      because (400 - 200) / 400 = 0.5
                rtt_variation = 0
                if new_rtt > old_rtt:
                    rtt_variation = (new_rtt - old_rtt) / float(new_rtt)
                else:
                    rtt_variation = (old_rtt - new_rtt) / float(old_rtt)
                if rtt_variation > self.rtt_variation_threshold:
                    to_be_changed.append((key, val_new))
                # TODO better handling of different devs to reach the
                # same neighbour

        # For each single change, call a "time_tick" method.
        #
        # The called method will:
        #  .evaluate (t-1)-like stuff (node_nb, ...)
        #  .if it is a new, check that it does not exist; otherwise exit.
        #  .if it is a changed_rem, check that it does exist; otherwise exit.
        #  .if it is a dead, check that it does exist; otherwise exit.
        #  .change data in structure without passing schedule.
        #  .call events (to Etp class) that must not be microfunc.
        #  .evaluate (t)-like stuff (node_nb, ...)
        #  .emit TIME_TICK event
        for key in to_be_deleted:
            self.store_delete_neigh(key)
        for key, val in to_be_added:
            self.store_add_neigh(key, val)
        for key, val in to_be_changed:
            self.store_changed_neigh(key, val)

        # returns an indication for next wait time to the radar
        if not self.missing_neighbour_keys:
            return 0
        return max(self.missing_neighbour_keys.values()) * \
                   self.increment_wait_time

    def reset_ntk_clients(self):
        """Reset connected TCPClients. To be used after hooking, to avoid
           using invalid sockets."""
        for key in self.ip_netid_table:
            ip, netid = key
            if ip in self.ntk_client:
                if self.ntk_client[ip].connected:
                    self.ntk_client[ip].close()

    def add(self, key):
        """Sends event for a new neighbour."""

        ip, netid = key

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, better not to emit any signal.
            stacktrace = get_stackframes()
            logging.debug('Neighbour.add: I\'m hooking, better not to emit any signal, while at ' + stacktrace)
            return

        val = self.ip_netid_table[key]

        is_in_my_net = netid == self.netid
        event_to_fire = 'NEIGH_NEW' if is_in_my_net else 'COLLIDING_NEIGH_NEW'
        if is_in_my_net:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', is now in my network.')
            logging.debug('ANNOUNCE: gw ' + str(key) + ' detected.')
            self.announce_gw(key)
        else:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', is a known neighbour but it '
                         'is not in my network.')

        # send a message notifying we added a node
        self.events.send(event_to_fire,
                         (val,))

    def delete(self, key, old_val, before_dead_link):
        """Sends event for a dead neighbour."""

        ip, netid = key

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, better not to emit any signal.
            stacktrace = get_stackframes()
            logging.debug('Neighbour.delete: I\'m hooking, better not to emit any signal, while at ' + stacktrace)
            return

        is_in_my_net = netid == self.netid
        event_to_fire = 'NEIGH_DELETED' if is_in_my_net else \
                        'COLLIDING_NEIGH_DELETED'
        if is_in_my_net:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', is no more in my network.')
            logging.debug('ANNOUNCE: gw ' + str(key) + ' removing.')
            self.announce_gw_removing(key)
        else:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', is no more a known neighbour anyway '
                         'it was not in my network.')

        # send a message notifying we deleted the node
        self.events.send(event_to_fire,
                         (old_val,
                          before_dead_link))

    def rem_change(self, key, old_rtt, before_changed_link):
        """Sends event for a changed rem neighbour."""

        ip, netid = key

        if self.ntkd_status.gonna_hook or self.ntkd_status.hooking:
            # I'm hooking, better not to emit any signal.
            stacktrace = get_stackframes()
            logging.debug('Neighbour.rem_change: I\'m hooking, better not to emit any signal, while at ' + stacktrace)
            return

        val = self.ip_netid_table[key]

        is_in_my_net = netid == self.netid
        event_to_fire = 'NEIGH_REM_CHGED' if is_in_my_net else \
                        'COLLIDING_NEIGH_REM_CHGED'
        if is_in_my_net:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', which is in my network, '
                         'changed its REM.')
        else:
            logging.info('Neighbour ip ' + ip_to_str(ip) + ', netid ' + 
                         str(netid) + ', which is *not* in my network, '
                         'changed its REM.')

        # send a message notifying the node's rtt changed
        self.events.send(event_to_fire,
                         (val, 
                          Rtt(old_rtt),
                          before_changed_link))

    @microfunc()
    def ip_netid_change(self, oldip, oldnetid, newip, newnetid):
        """Adds `newip' in the Neighbours as a copy of `oldip', then it
        removes `oldip'. The relative events are raised."""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        oldkey = (oldip, oldnetid)
        newkey = (newip, newnetid)
        if not oldkey in self.ip_netid_table:
            # probably our radar did not observed previously the ip that is 
            # changing, then leave this work to the next radar scan
            return

        # This neighbour was in our data structures and must be changed.

        # There won't be problems with max_neigh, as long as this method
        #  is atomic. That is, we MUST NOT pass schedule until the call
        #  that we will make to method "store_add_neigh". (Note: calling
        #  a time_tick function, since it is a microfunc with dispatcher,
        #  will not pass the schedule)

        # info
        logging.info('Change in our LAN: new neighbour ' + ip_to_str(newip) + 
                     ' in ' + str(newnetid))
        logging.info('                   replacing an old one... ' + 
                     ip_to_str(oldip) + ' in ' + str(oldnetid))
        # copy values from old ip_netid_table
        old_val = self.ip_netid_table[oldkey]
        old_ntk_client = self.get_ntk_client(oldip, oldnetid)
        # unmemorize
        self.store_delete_neigh(oldkey)
        # Attention: this removes "oldkey" from self.ip_netid_table. But 
        # current radar scan might already have put this "oldkey" in 
        # bcast_arrival_time. So...
        if oldkey in self.radar.bcast_arrival_time:
            logging.log(logging.ULTRADEBUG, 'ip_netid_change: removing '
                        'from scan...')
            del self.radar.bcast_arrival_time[oldkey]
        # memorize
        new_val = Neigh(bestdev=old_val.bestdev,
                        devs=old_val.devs,
                        ip=newip, netid=newnetid,
                        macs=old_val.macs,
                        ntkd_func=old_val.ntkd_func)
        self.store_add_neigh(newkey, new_val)

        # take care of self.ntk_client
        if self.netid != -1:
            if self.netid == newnetid and self.netid != oldnetid:
                self.ntk_client[newip] = rpc.TCPClient(ip_to_str(newip))
            if self.netid != newnetid and self.netid == oldnetid:
                del self.ntk_client[oldip]

        # Since the changes are only queued (in a time_tick serialized microfunc)
        # it is useless to test the values of self.ip_netid_table now.

    def call_ip_netid_change_broadcast_udp(self, oldip, oldnetid, 
                                           newip, newnetid):
        """Use BcastClient to call <broadcast> ip_netid_change"""
        devs = list(self.radar.broadcast.devs)
        rpc.UDP_broadcast_call(devs,'neighbour.ip_netid_change_broadcast_udp',
                               (oldip, oldnetid, newip, newnetid))

    def ip_netid_change_broadcast_udp(self, _rpc_caller, caller_id, oldip, 
                                      oldnetid, newip, newnetid):
        """Receives call for ip_netid_change_udp."""
        if rpc.UDP_broadcast_got_call(_rpc_caller, caller_id):
            self.ip_netid_change(oldip, oldnetid, newip, newnetid)

    def call_ip_netid_change_udp(self, neigh, oldip, oldnetid, newip, 
                                 newnetid):
        """Use BcastClient to call ip_netid_change"""
        devs = [neigh.bestdev[0]]
        nip = self.maproute.ip_to_nip(neigh.ip)
        netid = neigh.netid
        return rpc.UDP_call(nip, netid, devs, 'neighbour.ip_netid_change_udp',
                            (oldip, oldnetid, newip, newnetid))

    def ip_netid_change_udp(self, _rpc_caller, caller_id, callee_nip, 
                            callee_netid, oldip, oldnetid, newip, newnetid):
        """Returns the result of ip_netid_change to remote caller.
           caller_id is the random value generated by the caller for 
           this call.
            It is replied back to the LAN for the caller to recognize a reply 
            destinated to it.
           callee_nip is the NIP of the callee;
           callee_netid is the netid of the callee.
            They are used by the callee to recognize a request destinated 
            to it.
           """
        if self.maproute.me == callee_nip and self.netid == callee_netid:
            self.ip_netid_change(oldip, oldnetid, newip, newnetid)
            # Since it is micro, I will reply None
            rpc.UDP_send_reply(_rpc_caller, caller_id, None)

    def is_neigh_in_my_network(self, neigh):
        """Returns True if the passed Neigh is in my network."""
        return neigh.netid == self.netid

    def change_netid(self, new_netid):
        """Changes my network id.
           Handles events for neighbours that are NOW in my network.
        """
        logging.log(logging.ULTRADEBUG, 'change_netid: changing my netid '
                    'from ' + str(self.netid) + ' to ' + str(new_netid) + '.')
        # We should not need to send delete events for old companions.
        # Sets new netid
        self.netid = new_netid
        logging.info('change_netid: my netid is now ' + str(self.netid) + '.')
        # We DO need to send add events for new companions. We already have set the
        # netid, it won't change during the sending of the ETP.
        for neigh in self.neigh_list():
            key = (neigh.ip, neigh.netid)
            self.add(key)
        # take care of self.ntk_client
        for k in self.ntk_client.keys():
            del self.ntk_client[k]
        if self.netid != -1:
            for neigh in self.neigh_list():
                if self.netid == neigh.netid:
                    self.ntk_client[neigh.ip] = rpc.TCPClient(ip_to_str(neigh.ip))

class Radar(object):
    
    __slots__ = [ 'ntkd_status', 'time_tick_serializer', 'nic_manager', 'maproute',
                  'bouquet_numb', 'bcast_send_time', 'xtime',
                  'bcast_arrival_time', 'bcast_macs', 'max_bouquet', 'wait_time',
                  'broadcast', 'neigh', 'events',
                  'remotable_funcs', 'ntkd_id', 'radar_id', 'max_neigh',
                  'increment_wait_time', 'stopping', 'running_instances']

    def __init__(self, ntkd_status, time_tick_serializer, nic_manager, maproute, broadcast, xtime):
        """
            broadcast: an instance of the RPCBroadcast class to manage 
            broadcast sending xtime: a wrap.xtime module
        """

        self.ntkd_status = ntkd_status
        self.time_tick_serializer = time_tick_serializer
        self.nic_manager = nic_manager
        self.maproute = maproute

        self.xtime = xtime
        self.broadcast = broadcast

        # how many bouquet we have already sent
        self.bouquet_numb = 0
        # when we sent the broadcast packets
        self.bcast_send_time = 0
        # when the replies arrived
        self.bcast_arrival_time = {}
        # MACs gathered
        self.bcast_macs = {}
        # max_bouquet: how many packets does each bouquet contain?
        self.max_bouquet = settings.MAX_BOUQUET
        # wait_time: the time we wait for a reply, in seconds
        self.wait_time = settings.RADAR_WAIT_TIME
        # max_neigh: maximum number of neighbours we can have
        self.max_neigh = settings.MAX_NEIGH
        # our neighbours
        self.neigh = Neighbour(ntkd_status, time_tick_serializer, self, self.maproute, self.max_neigh, self.xtime)
        # Do I have to wait longer? in millis
        self.increment_wait_time = 0

        # Send a SCAN_DONE event each time a sent bouquet has been completely
        # collected
        self.events = Event(['SCAN_DONE'])

        self.stopping = False
        self.running_instances = []

        self.remotable_funcs = [self.reply, self.time_register]

        # this is needed to avoid to answer to myself
        self.ntkd_id = randint(0, 2**32-1)

    def run(self):
        # Before launching the microfunc,
        # make sure to note down that radar.run has been launched.

        # There should always be just one instance
        if self.running_instances:
            raise Exception('An instance of Radar is already running')

        self.running_instances.append(1)
        self._run()

    @microfunc(True)
    def _run(self):
        while not self.stopping:
            self.radar()
        self.running_instances.remove(1)
        if not self.running_instances:
            self.stopping = False

    def stop(self, event_wait = None):
        """ Stop the radar scanner """
        if self.running_instances:
            self.stopping = True
            while self.stopping:
                time.sleep(0.005)
                micro_block()

    def radar(self):
        """ Send broadcast packets and store the results in neigh """

        try:
            self.radar_id = randint(0, 2**32-1)
            logging.debug('radar scan %s' % self.radar_id)
            logging.debug('My netid is ' + str(self.neigh.netid))

            # we're sending the broadcast packets NOW
            self.bcast_send_time = self.xtime.time()

            # send all packets in the bouquet
            for i in xrange(self.max_bouquet):
                self.broadcast.radar.reply(self.ntkd_id, self.radar_id)

            # then wait
            self.xtime.swait(self.wait_time * 1000 + self.increment_wait_time)

            # test wether we are stopping
            if not self.stopping:
                # update the neighbours' ip_netid_table
                # Note: the neighbour manager will tell me if I have to wait
                #       longer than usual at the next scan.
                self.increment_wait_time = self.neigh.store(
                                            self.get_all_avg_rtt())

                # Send the event
                self.bouquet_numb += 1
                self.events.send('SCAN_DONE', (self.bouquet_numb,))
        except Exception, e:
            logging.warning('Exception ' + str(e) + ' while doing a radar' +
                          ' scan. We ignore it. Soon another scan.')

        # We're done. Reset.
        self.radar_reset()

    def radar_reset(self):
        ''' Clean the objects needed by radar()'''
        # Clean some stuff
        self.bcast_arrival_time = {}

        # Reset the broadcast sockets
        self.broadcast.reset()

    def reply(self, _rpc_caller, ntkd_id, radar_id):
        """ As answer we'll return our netid """

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if ntkd_id != self.ntkd_id:
            # If I am hooking I will not reply to radar scans from my 
            # neighbours
            # TODO is that ok?
            if self.neigh.netid != -1:
                bcc = rpc.BcastClient(devs=[_rpc_caller.dev], 
                                      xtimemod=self.xtime)
                receiving_mac = self.nic_manager[_rpc_caller.dev].mac
                try:
                    bcc.radar.time_register(radar_id, self.neigh.netid, receiving_mac)
                except:
                    logging.log(logging.ULTRADEBUG, 'Radar: Reply: '
                                'BcastClient ' + str(bcc) + ' with '
                                'dispatcher ' + 
                                repr(bcc.dev_sk[_rpc_caller.dev].dispatcher) +
                                ' error in rpc execution. Ignored.')

    def time_register(self, _rpc_caller, radar_id, netid, mac):
        """save each node's rtt"""

        # Implements "zombie" status
        if self.ntkd_status.zombie: raise ZombieException('I am a zombie.')

        if radar_id != self.radar_id:
            # drop. It isn't a reply to our current bouquet
            return

        ip = str_to_ip(_rpc_caller.ip)
        net_device = _rpc_caller.dev

        # this is the rtt
        time_elapsed = int((self.xtime.time() - self.bcast_send_time) / 2)
        # let's store it in the bcast_arrival_time table
        if (ip, netid) in self.bcast_arrival_time:
            if mac not in self.bcast_macs[(ip, netid)]:
                self.bcast_macs[(ip, netid)].append(mac)
            if net_device in self.bcast_arrival_time[(ip, netid)]:
                self.bcast_arrival_time[(ip, 
                                    netid)][net_device].append(time_elapsed)
            else:
                self.bcast_arrival_time[(ip, netid)][net_device] = \
                                                  [time_elapsed]
        else:
            self.bcast_arrival_time[(ip, netid)] = {}
            self.bcast_macs[(ip, netid)] = [mac]
            self.bcast_arrival_time[(ip, netid)][net_device] = [time_elapsed]
            logging.debug("Radar: IP %s from network %s detected", 
                          ip_to_str(ip), str(netid))

    def get_avg_rtt(self, ip, netid):
        """ ip: ip of the neighbour;
            netid: netid of the neighbour;
            Calculates the average rtt of the neighbour for each device

            Returns the ordered list [(dev, avgrtt)], the first element has
            the best average rtt.
        """

        devlist = []

        # for each NIC
        for dev in self.bcast_arrival_time[(ip, netid)]:
            avg = sum(self.bcast_arrival_time[(ip, netid)][dev]) / \
                    len(self.bcast_arrival_time[(ip, netid)][dev])
            devlist.append( (dev, avg) )

        # sort the devices, the best is the first
        def second_element((x,y)): return y
        devlist.sort(key=second_element)

        return devlist

    def get_all_avg_rtt(self):
        """ Calculate the average rtt of all the neighbours """

        all_avg = {}
        # for each (ip, netid)
        for (ip, netid) in self.bcast_arrival_time:
            devs = self.get_avg_rtt(ip, netid)
            macs = self.bcast_macs[(ip, netid)]
            all_avg[(ip, netid)] = Neigh(bestdev=devs[0], devs=dict(devs), 
                                         ip=ip, netid=netid, macs=macs,
                                         ntkd_func=self.neigh.get_ntk_client)
        return all_avg
