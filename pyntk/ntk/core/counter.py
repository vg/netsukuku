# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
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

from random import choice
import time as stdtime

import ntk.lib.rencode as rencode
from ntk.lib.rencode import serializable
from ntk.core.andna import hash_32bit_ip
from ntk.core.p2p import OptionalP2P
from ntk.lib.crypto import md5
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block, micro_current, micro_kill, start_tracking, stop_tracking
from ntk.wrap.xtime import while_condition, time, swait, TimeCapsule
from ntk.core.snsd import MAX_TTL_OF_REGISTERED, MAX_TTL_OF_NEGATIVE
from ntk.lib.rpc import TCPClient
from ntk.network.inet import ip_to_str
from ntk.lib.event import Event
from ntk.lib.log import ExpectableException

MAX_HOSTNAMES = 16

class CounterError(Exception): pass

class CounterExpectableException(ExpectableException): pass

class CounterAuthRecord:
    def __init__(self, pubk, nip, hnames, ttl):
        # PublicKey pubk = public key of registrar
        self.pubk = pubk
        # NIP nip = NIP of registrar
        self.nip = nip[:]
        # sequence<string> hnames = hostnames registered
        self.hnames = hnames[:]
        # int ttl = millisec to expiration
        #           if 0 => MAX_TTL_OF_REGISTERED
        if ttl == 0 or ttl > MAX_TTL_OF_REGISTERED:
            ttl = MAX_TTL_OF_REGISTERED
        self.expires = time() + ttl

    def __repr__(self):
        ret = '<CounterAuthRecord: (pubk ' + self.pubk.short_repr() + \
                ', nip ' + str(self.nip) + \
                ', hnames ' + str(self.hnames) + \
                ', ttl ' + str(self.get_ttl()) + ')>'
        return ret

    def get_ttl(self):
        return self.expires - time()

    def _pack(self):
        ttl = self.get_ttl()
        if ttl == 0: ttl = -1
        return (self.pubk, self.nip[:], self.hnames[:], ttl)

serializable.register(CounterAuthRecord)

class Counter(OptionalP2P):

    pid = 2

    def __init__(self, ntkd_status, keypair, radar, maproute, p2pall):
        OptionalP2P.__init__(self, ntkd_status, radar, maproute, Counter.pid)
        # period starting from registration time within which
        # the hostname must be updated
        self.my_keys = keypair
        self.pubk = self.my_keys.get_pub_key()
        self.p2pall = p2pall

        # let's register ourself in p2pall
        self.p2pall.p2p_register(self)
        self.p2pall.events.listen('P2P_HOOKED', self.counter_hook)

        self.events = Event(['COUNTER_HOOKED'])

        # From the moment I change my NIP up to the moment I have hooked,
        # we don't want to answer to check/reset requests.
        self.wait_counter_hook = True
        self.micro_to_kill = {}
        self.maproute.events.listen('ME_CHANGED', self.enter_wait_counter_hook)

        # The counter cache = { (pubk, tuple(nip)): CounterAuthRecord, ... }
        self.cache = {}
        # Note: tuple is hashable, list is not.

        # Resolved Cache for reverse_resolution = { tuple(nip): [(hostname, TimeCapsule(ttl)), ...], ... }
        self.resolved_reverse_resolution = {}

        self.remotable_funcs += [self.check,
                                 self.confirm_your_request,
                                 self.cache_getall,
                                 self.reset_nip,
                                 self.reverse_resolve]

    def set_andna(self, andna):
        self.andna = andna

    def enter_wait_counter_hook(self, *args):
        self.wait_counter_hook = True
        while any(self.micro_to_kill):
            for k in self.micro_to_kill.keys():
                logging.debug('COUNTER: enter_wait_counter_hook: killing...')
                micro_kill(self.micro_to_kill[k])

    def reset(self):
        ''' Resets all data. Invoked when hooked in a new network.
        '''
        self.cache = {}
        self.resolved_reverse_resolution = {}

    @microfunc(True, keep_track=1) 
    def counter_hook(self):
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'counter_hook' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['counter_hook'])
        try:
            self.micro_to_kill['counter_hook'] = micro_current()

            # clear old cache
            self.reset()
            logging.debug('COUNTER: resetted.')

            # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
            #  from whom I have to retrieve the cache
            bunch = [self.maproute.me[:]]
            bunch_not_me = [n for n in bunch if n != self.maproute.me]

            for cache_nip in bunch_not_me:
                # TODO Contact the various nip in the bunch in parallel. But
                #      be careful. We must continue after all have finished
                #      (completed or failed) and we must ensure the atomicity
                #      of write.
                # TODO Use TCPClient or P2P ?
                remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(cache_nip)))
                logging.debug('COUNTER: getting cache from ' + str(cache_nip))
                try:
                    cache_from_nip = remote.counter.cache_getall()
                    for key in cache_from_nip.keys():
                        # TODO perhaps we don't have this key, but we already
                        #      tried and we refused because we're not in the
                        #      bunch. In this case we should avoid to start a
                        #      find_nearest again.
                        # Do I have already this record?
                        if key not in self.cache:
                            # No. Do I am the right hash for this record?
                            pubk, tuple_nip = key
                            nip = list(tuple_nip)
                            hnode_nip = self.peer(key=nip).get_hash_nip()

                            # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
                            hnode_bunch = [hnode_nip]

                            if self.maproute.me in hnode_bunch:
                                # Yes. Memorize it.
                                self.cache[key] = cache_from_nip[key]
                                logging.debug('COUNTER: got cache for ' + \
                                        str(nip))
                    logging.debug('COUNTER: executed cache from ' + \
                            str(cache_nip))
                except Exception, e:
                    logging.debug('COUNTER: getting cache from ' + \
                            str(cache_nip) + ' got exception ' + repr(e))

            # Now I can answer to requests.
            self.events.send('COUNTER_HOOKED', ())
            self.wait_counter_hook = False
            logging.info('Counter: Emit signal COUNTER_HOOKED.')

            # Communicate to one Counter Node that I am the new holder
            # of this NIP.
            logging.debug('COUNTER: starting keeping of my identity.')
            self.reset_my_counter_node()

        finally:
            del self.micro_to_kill['counter_hook']

    @microfunc(True, keep_track=1)
    def reset_my_counter_node(self):
        ''' Asks to my counter node to set my pubk as the
            current holder, and declare any hostname, and keep updating.
        '''
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'andna_hook' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['reset_my_counter_node'])
        try:
            self.micro_to_kill['reset_my_counter_node'] = micro_current()

            # TODO: this way of obtaining my hostnames has to be reworked.
            import ntk.lib.misc as misc
            from ntk.config import settings
            hnames = []
            try:
                hostname = misc.get_hostname()
                hnames.append(hostname)
            except Exception, e:
                logging.debug('Counter: reset_my_counter_node: get_hostname got ' + repr(e))
            for line in misc.read_nodes(settings.SNSD_NODES_PATH):
                result, data = misc.parse_snsd_node(line)
                if not result:
                    raise ImproperlyConfigured("Wrong line in "+str(settings.SNSD_NODES_PATH))
                hnames.append(data[1])

            ttl_holder = TimeCapsule(0)
            while True:
                # Wait that there is something to do
                def something_to_do():
                    return ttl_holder.get_ttl() <= 0
                while_condition(something_to_do, wait_millisec=100)
                # Do [again] the requests.
                if ttl_holder.get_ttl() <= 0:
                    ttl = 0
                    try:
                        logging.debug('Counter: declare my hostnames to the Counter.')
                        # the return value is the ttl of the registration
                        ttl = self.ask_reset_nip(hnames)
                    except Exception, e:
                        logging.debug('Counter: reset_my_counter_node: reset_nip got ' + repr(e))
                        # the failure may be temporary
                        ttl = 1000 * 60 * 2 # 2 minutes in millisec
                    logging.debug('Counter: my counter node will be ok for ' + str(ttl) + ' msec.')
                    ttl_holder = TimeCapsule(ttl)

        finally:
            del self.micro_to_kill['reset_my_counter_node']

    ########## remotable methods directly called to a NIP

    def confirm_your_request(self, nip, pubk):
        # This P2P-remotable method is called directyly to a certain NIP in order
        # to verify that he is the sender of a certain request. The method asks to
        # confirm that the NIP is the right one, that the public key is of this node.
        return nip == self.maproute.me and \
               pubk == self.pubk

    ########## Helper functions used as a client of Counter, to send requests.

    def ask_reset_nip(self, hnames):
        ''' Declare I am the holder of this nip.
            I declare all the hostnames I will try to register.
        '''
        logging.debug('COUNTER: ask_reset_nip(' + str(hnames) + ')')
        # calculate hash
        hash_node = self.peer(key=self.maproute.me[:])
        logging.debug('COUNTER: ask_reset_nip: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        nip = self.maproute.me[:]
        # sign the request and attach the public key
        signature = self.my_keys.sign(rencode.dumps((nip, hnames)))
        pubk = self.pubk
        return hash_node.reset_nip(pubk, nip, hnames, signature)

    def ask_check(self, registrar_nip, pubk, hostname):
        """ Register or update the name for the specified service number """
        logging.debug('COUNTER: ask_check' + str((registrar_nip, pubk, hostname)))
        # calculate hash
        hash_node = self.peer(key=registrar_nip)
        logging.debug('COUNTER: ask_check: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        logging.debug('COUNTER: ask_check: request check')
        return hash_node.check(registrar_nip, pubk, hostname)

    def check_expirations_resolved_cache(self):
        # Remove the expired entries from the resolved cache
        logging.debug('COUNTER: cleaning expired entries - if any - from our resolved cache...')
        logging.debug('COUNTER: self.resolved_reverse_resolution was ' + str(self.resolved_reverse_resolution))
        for key, data_cached in self.resolved_reverse_resolution.items():
            # if any of the TTL expired, we must remove all the cached list
            expired = False
            for hostname, timecapsule_ttl in data_cached:
                if timecapsule_ttl.get_ttl() < 0:
                    expired = True
                    break
            if expired:
                self.resolved_reverse_resolution.pop(key)
                logging.debug('COUNTER: cleaned ' + str(key) + ' from resolved_reverse_resolution cache')
        logging.debug('COUNTER: self.resolved_reverse_resolution is ' + str(self.resolved_reverse_resolution))

    def ask_reverse_resolution(self, nip):
        ''' Ask for reverse resolution of nip
        '''
        logging.debug('COUNTER: ask_reverse_resolution(' + str(nip) + ')')
        # Remove the expired entries from the resolved cache
        self.check_expirations_resolved_cache()

        # first try to resolve locally
        if self.resolved_reverse_resolution.has_key(tuple(nip)):
            data_cached = self.resolved_reverse_resolution[tuple(nip)]
            # data_cached = [(hostname, TimeCapsule(ttl)), ...]
            data_to_return = []
            for hostname, timecapsule_ttl in data_cached:
                data_to_return.append((hostname, timecapsule_ttl.get_ttl()))
            return data_to_return
        # else call the remote hash node
        # calculate hash
        hash_node = self.peer(key=nip)
        logging.debug('COUNTER: ask_reverse_resolution: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        logging.debug('COUNTER: ask_reverse_resolution: request reverse resolution')
        data = hash_node.reverse_resolve(nip)
        if any(data):
            # data = [(hostname, ttl), ...]
            data_to_cache = []
            for hostname, ttl in data:
                data_to_cache.append((hostname, TimeCapsule(ttl)))
            self.resolved_reverse_resolution[tuple(nip)] = data_to_cache
        return data

    ########## Remotable and helper functions used as a server, to serve requests.

    def check_expirations(self):
        # Remove the expired entries from the COUNTER cache
        logging.debug('COUNTER: cleaning expired entries - if any - from our COUNTER cache...')
        logging.debug('COUNTER: self.cache was ' + str(self.cache))
        for key, auth_record in self.cache.items():
            pubk, nip = auth_record.pubk, auth_record.nip
            if auth_record.get_ttl() <= 0:
                self.cache.pop(key)
                logging.debug('COUNTER: cleaned entry (pubk ' + pubk.short_repr() + \
                        ', nip ' + str(nip) + ')')
        logging.debug('COUNTER: self.cache is ' + str(self.cache))

    def reset_nip(self, pubk, nip, hnames, signature, forward=True):
      ''' This pubk is declaring it is the holder of this nip.
          It wants to declare all the hostnames.
      '''
      start_tracking()
      try:
        if len(hnames) > MAX_HOSTNAMES:
            raise CounterError, 'Too many hostnames'

        # If we recently changed our NIP (we hooked) we wait to finish
        # an counter_hook before answering a check request.
        def exit_func():
            return not self.wait_counter_hook
        logging.debug('COUNTER: reset_nip: waiting counter_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('COUNTER: reset_nip: We are correctly hooked.')
        # We are correctly hooked.

        # Remove the expired entries from the COUNTER cache
        self.check_expirations()

        # first the hash check
        logging.debug('COUNTER: reset_nip: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=nip)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('COUNTER: reset_nip: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
        bunch = [hash_nip, self.maproute.me]

        check_hash = self.maproute.me in bunch

        if not check_hash:
            logging.info('COUNTER: reset_nip: hash is NOT verified. Raising exception.')
            raise CounterError, 'Hash verification failed'
        logging.debug('COUNTER: reset_nip: hash is verified.')

        # Check that the request is signed with privk of this pubk
        logging.debug('COUNTER: reset_nip: authenticating the request...')
        if not pubk.verify(rencode.dumps((nip, hnames)), signature):
            raise CounterError('Request authentication failed')
        logging.debug('COUNTER: reset_nip: request authenticated')

        # Check that the request is coming from this NIP
        logging.debug('COUNTER: reset_nip: verifying the request came from this nip...')
        remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(nip)))
        try:
            remote_resp = remote.counter.confirm_your_request(nip, pubk)
        except Exception, e:
            raise CounterError, 'Asking confirmation to nip ' + str(nip) + ' got ' + repr(e)
        if not remote_resp:
            logging.info('COUNTER: reset_nip: nip is NOT verified. Raising exception.')
            raise CounterError, 'Request not originating from nip ' + str(nip)
        logging.debug('COUNTER: reset_nip: nip verified')

        # store
        record = CounterAuthRecord(pubk, nip, hnames, 0)
        ret = record.get_ttl()
        self.cache[(pubk, tuple(nip))] = record
        if forward:
            # forward the entry to the bunch
            bunch_not_me = [n for n in bunch if n != self.maproute.me]
            logging.debug('COUNTER: reset_nip: forward_registration to ' + str(bunch_not_me))
            self.forward_registration_to_set(bunch_not_me, \
                    (pubk, nip, hnames, signature))
        return ret
      finally:
        stop_tracking()

    @microfunc(True)
    def forward_registration_to_set(self, to_set, args_to_reset_nip):
        for to_nip in to_set:
            self.forward_registration_to_nip(to_nip, args_to_reset_nip)

    @microfunc(True, keep_track=1)
    def forward_registration_to_nip(self, to_nip, args_to_reset_nip):
        """ Forwards registration request to another counter node in the bunch. """
        try:
            logging.debug('COUNTER: forwarding registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)))
            # TODO Use TCPClient or P2P ?
            remote = self.peer(hIP=to_nip)
            args_to_reset_nip = \
                    tuple(list(args_to_reset_nip) + [False])
            resp = remote.reset_nip(*args_to_reset_nip)
            logging.debug('COUNTER: forwarded registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)) + \
                    ' got ' + str(resp))
        except Exception, e:
            logging.warning('COUNTER: forwarded registration request to ' + \
                    str(to_nip) + \
                    ' got exception ' + repr(e))

    def check(self, registrar_nip, pubk, hostname):
        """ Return a tuple like (True/False, TTL) """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an counter_hook before answering a check request.
        def exit_func():
            return not self.wait_counter_hook
        logging.debug('COUNTER: waiting counter_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('COUNTER: We are correctly hooked.')
        # We are correctly hooked.

        # Remove the expired entries from the COUNTER cache
        self.check_expirations()

        logging.debug('COUNTER: check' + str((registrar_nip, pubk, hostname)))
        # first the hash check
        logging.debug('COUNTER: check: verifying that I am the right hash...')
        check_hash = self.maproute.me == self.H(self.h(registrar_nip))

        if not check_hash:
            logging.info('COUNTER: check: hash is NOT verified. Raising exception.')
            raise CounterError, 'Hash verification failed'
        logging.debug('COUNTER: check: hash is verified.')

        key = pubk, tuple(registrar_nip)
        if key in self.cache:
            counter_auth_rec = self.cache[key]
            if hostname in counter_auth_rec.hnames:
                return True, counter_auth_rec.get_ttl()
        return False, 0

    def reverse_resolve(self, nip):
        """ Return a list of pairs like (hostname, TTL) """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an counter_hook before answering a reverse_resolve request.
        def exit_func():
            return not self.wait_counter_hook
        logging.debug('COUNTER: waiting counter_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('COUNTER: We are correctly hooked.')
        # We are correctly hooked.

        # Remove the expired entries from the COUNTER cache
        self.check_expirations()

        logging.debug('COUNTER: reverse_resolve(' + str(nip) + ')')
        # first the hash check
        logging.debug('COUNTER: reverse_resolve: verifying that I am the right hash...')
        check_hash = self.maproute.me == self.H(self.h(nip))

        if not check_hash:
            logging.info('COUNTER: reverse_resolve: hash is NOT verified. Raising exception.')
            raise CounterError, 'Hash verification failed'
        logging.debug('COUNTER: reverse_resolve: hash is verified.')

        ret = []
        hname_no_dup = []
        for key in self.cache:
            # key = pubk, tuple(registrar_nip)
            if key[1] == tuple(nip):
                counter_auth_rec = self.cache[key]
                for hostname in counter_auth_rec.hnames:
                    # do not emit duplicates
                    if hostname in hname_no_dup: break
                    hname_no_dup.append(hostname)
                    logging.debug('COUNTER: reverse_resolve: nip ' + str(nip) + ' asked for ' + hostname)
                    # hostname has been requested by this nip. Has it really got?
                    registrar_nip, registrar_ttl = self.andna.ask_registrar_nip(hostname)
                    if registrar_nip == nip:
                        logging.debug('COUNTER: reverse_resolve: ... and it has got it.')
                        ret.append((hostname, min(counter_auth_rec.get_ttl(), registrar_ttl)))
                    else:
                        logging.debug('COUNTER: reverse_resolve: ... but it hasn\'t got it.')
                break
        logging.debug('COUNTER: reverse_resolve: returns ' + str(ret))
        return ret

    def cache_getall(self):
        ''' Returns the cache of authoritative records.
        '''
        if self.wait_counter_hook:
            raise CounterExpectableException, 'Counter is hooking. Request not valid.'
        return self.cache
    
    def h(self, nip):
        """ Retrieve an IP from the NIP """
        # TODO check!
        return hash_32bit_ip(md5(str(nip)), self.maproute.levels, 
                             self.maproute.gsize)
