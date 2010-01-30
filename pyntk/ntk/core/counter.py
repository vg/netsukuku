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

import time as stdtime

import ntk.lib.rencode as rencode
from ntk.lib.rencode import serializable
from ntk.core.andna import hash_32bit_ip
from ntk.core.p2p import OptionalP2P
from ntk.lib.crypto import md5
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block, micro_current, micro_kill, start_tracking, stop_tracking
from ntk.wrap.xtime import (now, timestamp_to_data, today, days, 
                            while_condition, time)
from ntk.core.snsd import MAX_TTL_OF_REGISTERED, MAX_TTL_OF_NEGATIVE
from ntk.lib.rpc import TCPClient
from ntk.network.inet import ip_to_str
from ntk.lib.event import Event
from ntk.lib.log import ExpectableException

MAX_HOSTNAMES = 16

class CounterError(Exception): pass

class CounterExpectableException(ExpectableException): pass

class HostnameRecord:
    def __init__(self, hostname, ttl):
        # string hostname = hostname
        self.hostname = hostname
        # int ttl = millisec to expiration
        #           if 0 => MAX_TTL_OF_REGISTERED
        if ttl == 0 or ttl > MAX_TTL_OF_REGISTERED:
            ttl = MAX_TTL_OF_REGISTERED
        self.expires = time() + ttl

    def __repr__(self):
        pubk_str = 'None'
        if self.pubk is not None:
            pubk_str = self.pubk.short_repr()
        ret = '<HostnameRecord: (hostname ' + str(self.hostname) + \
                ', ttl ' + str(self.get_ttl()) + ')>'
        return ret

    def get_ttl(self):
        return self.expires - time()

    def set_ttl(self, ttl=0):
        if ttl == 0 or ttl > MAX_TTL_OF_REGISTERED:
            ttl = MAX_TTL_OF_REGISTERED
        return self.expires - time()

    def _pack(self):
        return (self.hostname, self.get_ttl())

serializable.register(HostnameRecord)

class CounterAuthRecord:
    def __init__(self, pubk, nip, reg_hostnames):
        # PublicKey pubk = public key of registrar
        self.pubk = pubk
        # NIP nip = NIP of registrar
        self.nip = nip[:]
        # dict<hostname,sequence<HostnameRecord>> reg_hostnames = the hostnames granted to the registrar
        self.reg_hostnames = {}
        self.reg_hostnames.update(reg_hostnames)

    def __repr__(self):
        ret = '<CounterAuthRecord: (pubk ' + self.pubk.short_repr() + \
                ', nip ' + str(self.nip) + \
                ', reg_hostnames ' + str(self.reg_hostnames) + ')>'
        return ret

    def check_expirations(self):
        # Remove the expired entries from this structure
        for hname, hname_record in self.reg_hostnames.items():
            if hname_record.expires < time():
                self.reg_hostnames.pop(hname)

    def store(self, hname):
        """ Creates or updates a registration. Returns wether we accept. """
        # Remove expired entries
        self.check_expirations()
        # The registrar wants to register/update hname.
        # If I had hname, I must reset its TTL.
        # Otherwise I have to check the limit, and accept or reject.
        if hname in self.reg_hostnames:
            self.reg_hostnames[hname].set_ttl()
            return True
        elif len(self.reg_hostnames) >= MAX_HOSTNAMES:
            return False
        else:
            self.reg_hostnames[hname] = HostnameRecord(hname, 0)
            return True

    def has_any(self):
        self.check_expirations()
        return len(self.reg_hostnames) > 0

    def _pack(self):
        return (self.pubk, self.nip, self.reg_hostnames)

serializable.register(CounterAuthRecord)

class Counter(OptionalP2P):

    pid = 2

    def __init__(self, ntkd_status, keypair, radar, maproute, p2pall):
        OptionalP2P.__init__(self, ntkd_status, radar, maproute, Counter.pid)
        # period starting from registration time within which
        # the hostname must be updated
        self.my_keys = keypair
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

        self.remotable_funcs += [self.check,
                                self.cache_getall,
                                self.reset_nip]

    def enter_wait_counter_hook(self, *args):
        self.wait_counter_hook = True
        while any(self.micro_to_kill):
            for k in self.micro_to_kill.keys():
                logging.debug('COUNTER: enter_wait_counter_hook: killing...')
                micro_kill(self.micro_to_kill[k])

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

            self.events.send('COUNTER_HOOKED', ())
            self.wait_counter_hook = False

        finally:
            del self.micro_to_kill['counter_hook']

    def reset_my_counter_node(self):
        ''' Asks to my counter node to set my pubk as the
            new holder, and reset any hostname. The counter node(s)
            will ask for confirmation.
        '''
        pass #TODO contact counter node and call reset_nip
        # NOTE here I am the client, I don't need to test wait_counter_hook
        # But if I get an exception by the remote, I need to try again
        # forever. This has to be completed before trying to register
        # hostnames. This is in the same tasklet of andna_hook, which should
        # be a sort-of-restartable microfunc.

    def reset_nip(self, pubk, nip):
        ''' This pubk is declaring it is the holder of this nip.
            It wants to reset all the hostnames.
        '''
        pass #TODO reset self.cache[pubk, tuple(nip)]
        # NOTE here I am the client, I DO need to test wait_counter_hook

    def reset(self):
        ''' Resets all data. Invoked when hooked in a new network.
        '''
        self.cache = {}

    def check_expirations(self):
        # Remove the expired entries from the COUNTER cache
        logging.debug('COUNTER: cleaning expired entries - if any - from our COUNTER cache...')
        for key, auth_record in self.cache.items():
            pubk, nip = auth_record.pubk, auth_record.nip
            if not auth_record.has_any():
                self.cache.pop(key)
                logging.debug('COUNTER: cleaned entry (pubk ' + pubk.short_repr() + \
                        ', nip ' + str(self.nip) + ')')

    def check(self, sender_nip, pubk, hostname, serv_key, IDNum,
                       snsd_record, signature,
                       forward=True):
      """ Return a tuple like (True/False, updates) """
      start_tracking()
      try:
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

        # first the hash check
        logging.debug('COUNTER: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=sender_nip)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('COUNTER: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
        bunch = [hash_nip, self.maproute.me]

        check_hash = self.maproute.me in bunch

        if not check_hash:
            logging.info('COUNTER: hash is NOT verified. Raising exception.')
            raise CounterError, 'Hash verification failed'
        logging.debug('COUNTER: hash is verified.')

        # Check that the request is signed with privk of this pubk
        logging.debug('COUNTER: authenticating the request...')
        if not pubk.verify(rencode.dumps((sender_nip, hostname, serv_key, IDNum,
                       snsd_record)), signature):
            raise CounterError('Request authentication failed')
        logging.debug('COUNTER: request authenticated')

        # Check that the request is coming from this NIP
        logging.debug('COUNTER: verifying the request came from this nip...')
        remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(sender_nip)))
        try:
            remote_resp = remote.andna.confirm_your_request(sender_nip, pubk, hostname)
        except Exception, e:
            raise CounterError, 'Asking confirmation to nip ' + str(sender_nip) + ' got ' + repr(e)
        if not remote_resp:
            logging.info('COUNTER: nip is NOT verified. Raising exception.')
            raise CounterError, 'Request not originating from nip ' + str(sender_nip)
        logging.debug('COUNTER: nip verified')

        # Retrieve data, update data, prepare response
        logging.debug('COUNTER: processing request...')
        tuple_nip = tuple(sender_nip)
        if (pubk, tuple_nip) not in self.cache:
            self.cache[(pubk, tuple_nip)] = CounterAuthRecord(pubk, sender_nip, {})
        ret = self.cache[(pubk, tuple_nip)].store(hostname)
        # Is it accepted?
        if ret:
            if forward:
                # forward the entry to the bunch
                bunch_not_me = [n for n in bunch if n != self.maproute.me]
                logging.debug('COUNTER: forward_registration to ' + str(bunch_not_me))
                self.forward_registration_to_set(bunch_not_me, \
                        (sender_nip, pubk, hostname, \
                         serv_key, IDNum, snsd_record, signature))

        logging.debug('COUNTER: returning (ret, IDNum) = ' + str((ret, IDNum)))
        return (ret, IDNum)
      finally:
        stop_tracking()

    @microfunc(True)
    def forward_registration_to_set(self, to_set, args_to_check):
        for to_nip in to_set:
            self.forward_registration_to_nip(to_nip, args_to_check)

    @microfunc(True, keep_track=1)
    def forward_registration_to_nip(self, to_nip, args_to_check):
        """ Forwards registration request to another hash node in the bunch. """
        try:
            logging.debug('COUNTER: forwarding registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)))
            # TODO Use TCPClient or P2P ?
            remote = self.peer(hIP=to_nip)
            args_to_check = \
                    tuple(list(args_to_check) + [False])
            resp = remote.check(*args_to_check)
            logging.debug('COUNTER: forwarded registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)) + \
                    ' got ' + str(resp))
        except Exception, e:
            logging.warning('COUNTER: forwarded registration request to ' + \
                    str(to_nip) + \
                    ' got exception ' + repr(e))

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
