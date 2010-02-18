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

from random import choice, randint
import time as stdtime

from ntk.config import settings
import ntk.lib.rencode as rencode
from ntk.core.p2p import OptionalP2P
from ntk.core.snsd import SnsdAuthRecord, SnsdResolvedRecord, AndnaAuthRecord, AndnaResolvedRecord
from ntk.core.snsd import MAX_TTL_OF_NEGATIVE
from ntk.lib.crypto import md5, fnv_32_buf, PublicKey
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.log import log_exception_stacktrace
from ntk.lib.micro import microfunc, micro_block, micro_current, micro_kill, start_tracking, stop_tracking
from ntk.lib.misc import is_ip
from ntk.lib.rencode import serializable
from ntk.network.inet import ip_to_str, str_to_ip
from ntk.wrap.xtime import swait, time, while_condition, TimeCapsule
from ntk.core.snsd import AndnaError
from ntk.lib.rpc import TCPClient
from ntk.lib.log import ExpectableException

# TODO:
# - keep the public key of my neighbours on file, adding them path 
#   to /etc/netsukuku/snsd_nodes
# - add support for IPv6 (hash function, ecc)

# The serv_key to use for the main registration of the hostname
NULL_SERV_KEY = 0
# in futuro potrebbe essere None
# Make a serv_key from a string
def make_serv_key(str_value):
    if str_value == 'NULL':
        return NULL_SERV_KEY
    return int(str_value)
    # in futuro potrebbe essere (serv_name, serv_proto)
    # ad esempio a partire da '_www._tcp'

# Used to calculate the range of authoritative sources for a record
# associated to a given NIP
ANDNA_DUPLICATION = 10

# Used to spread the database entries to various parts of the network
# and to supply load-balancing for resolutions
ANDNA_SPREAD = 10

MAX_HOSTNAME_LEN = 256
MAX_ANDNA_QUEUE = 5

class AndnaExpectableException(ExpectableException): pass

class Andna(OptionalP2P):
    
    pid = 3
    
    def __init__(self, ntkd_status, keypair, counter, radar, maproute, p2pall):
        OptionalP2P.__init__(self, ntkd_status, radar, maproute, Andna.pid)
        
        self.p2pall = p2pall
        
        # let's register ourself in p2pall
        self.p2pall.p2p_register(self)

        self.events = Event(['ANDNA_HOOKED'])

        # From the moment I change my NIP up to the moment I have hooked,
        # we don't want to answer to registration/resolution requests.
        self.wait_andna_hook = True
        self.micro_to_kill = {}
        self.maproute.events.listen('ME_CHANGED', self.enter_wait_andna_hook)

        # The andna_hook will start after the counter has hooked.
        self.counter = counter
        self.counter.events.listen('COUNTER_HOOKED', self.andna_hook)

        self.my_keys = keypair

        # Appended requests = { hostname: 
        #                       sequence<args_tuple_passed_to_register>, ... }
        self.request_queue = {}

        # Andna Cache = { hostname: AndnaAuthRecord(), ... }
        self.cache = {} 

        # Local Cache = { hostname: (expires, updates), ... }
        self.local_cache = {}

        # Resolved Cache = { (hostname, serv_key): AndnaResolvedRecord(), ... }
        self.resolved = {}

        # Resolved Cache for registrar_nip = { hostname: (nip, TimeCapsule(ttl)), ... }
        self.resolved_registrar_nip = {}
    
        self.remotable_funcs += [self.register_hostname_main,
                                 self.register_hostname_spread,
                                 self.reply_resolve,
                                 self.get_registrar_pubk,
                                 self.reply_queued_registration,
                                 self.get_auth_cache,
                                 self.reply_registrar_nip]

    def enter_wait_andna_hook(self, *args):
        self.wait_andna_hook = True
        while any(self.micro_to_kill):
            for k in self.micro_to_kill.keys():
                logging.debug('ANDNA: enter_wait_andna_hook: killing...')
                micro_kill(self.micro_to_kill[k])

    def reset(self):
        self.request_queue = {}
        self.cache = {}
        self.local_cache = {}
        self.resolved = {}
        self.resolved_registrar_nip = {}

    @microfunc(True, keep_track=1)
    def andna_hook(self):
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'andna_hook' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['andna_hook'])
        try:
            self.micro_to_kill['andna_hook'] = micro_current()

            # clear old cache
            self.reset()
            logging.debug('ANDNA: andna_hook: resetted.')

            if settings.ANDNA_REPLICA_ACTIVATED:
                # We find 2 bunches of ANDNA_DUPLICATION*2 elements near to us, one
                # going up and one going down.
                # Note: We don't want ourself in the bunches.
                # For each one of them, ask for the cache to the element 1 and to the
                # element ANDNA_DUPLICATION+1. In case of error try 2, 3, ...
                # At the end we have the cache from:
                #  cache_1) myself-ANDNA_DUPLICATION-1
                #  cache_2) myself-1
                #  cache_3) myself+1
                #  cache_4) myself+ANDNA_DUPLICATION+1
                # We accept a record that is in 2 and in 3.
                # We accept a record that is in 2 and not in 1.
                # We accept a record that is in 3 and not in 4.

                bunch_up = self.find_nearest_exec(self.maproute.me[:], ANDNA_DUPLICATION*2+1, \
                            self.maproute.levels, path=1)
                logging.debug('ANDNA: andna_hook: bunch_up = ' + str(bunch_up))
                if self.maproute.me in bunch_up:
                    bunch_up.remove(self.maproute.me)
                bunch_down = self.find_nearest_exec(self.maproute.me[:], ANDNA_DUPLICATION*2+1, \
                            self.maproute.levels, path=-1)
                logging.debug('ANDNA: andna_hook: bunch_down = ' + str(bunch_down))
                if self.maproute.me in bunch_down:
                    bunch_down.remove(self.maproute.me)
                # Extreme cases:
                #  0 participating nodes: do nothing and we are hooked.
                #  less than ANDNA_DUPLICATION*2 participating nodes: accept all the records
                #      from any one of them.
                if len(bunch_up) == 0:
                    logging.debug('ANDNA: andna_hook: I am alone')
                else:
                    if len(bunch_up) < ANDNA_DUPLICATION*2:
                        logging.debug('ANDNA: andna_hook: we are few, let\'s get all')
                        accept_cache, accept_queue = [], []
                        for cache_nip in bunch_up:
                            try:
                                remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(cache_nip)))
                                accept_cache, accept_queue = remote.andna.get_auth_cache()
                                # succeded
                                logging.debug('ANDNA: andna_hook: found a cache')
                                break
                            except Exception, e:
                                logging.debug('ANDNA: andna_hook: getting cache from ' + \
                                    str(cache_nip) + ' got exception ' + repr(e))
                        # We accept all records.
                        for hname in accept_cache:
                            # Do I have already this record?
                            if hname not in self.cache:
                                # No. Memorize it.
                                self.cache[hname] = accept_cache[hname]
                                logging.debug('ANDNA: andna_hook: got cache for ' + \
                                        str(hname))
                        for hname in accept_queue:
                            # Do I have already this record?
                            if hname not in self.request_queue:
                                # No. Memorize it.
                                self.request_queue[hname] = accept_queue[hname]
                                logging.debug('ANDNA: andna_hook: got request_queue for ' + \
                                        str(hname))
                        logging.debug('ANDNA: andna_hook: finished')
                    else:
                        cache_1 = None
                        cache_2 = None
                        cache_3 = None
                        cache_4 = None
                        request_queue_1 = None
                        request_queue_2 = None
                        request_queue_3 = None
                        request_queue_4 = None
                        for i in xrange(ANDNA_DUPLICATION):
                            if cache_1 is None:
                                try:
                                    remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(bunch_down[ANDNA_DUPLICATION+i])))
                                    cache_1, request_queue_1 = remote.andna.get_auth_cache()
                                    # succeded
                                    logging.debug('ANDNA: andna_hook: found a cache_1')
                                except Exception, e:
                                    logging.debug('ANDNA: andna_hook: getting cache from ' + \
                                        str(bunch_down[ANDNA_DUPLICATION+i]) + ' got exception ' + repr(e))
                            if cache_2 is None:
                                try:
                                    remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(bunch_down[i])))
                                    cache_2, request_queue_2 = remote.andna.get_auth_cache()
                                    # succeded
                                    logging.debug('ANDNA: andna_hook: found a cache_2')
                                except Exception, e:
                                    logging.debug('ANDNA: andna_hook: getting cache from ' + \
                                        str(bunch_down[i]) + ' got exception ' + repr(e))
                            if cache_3 is None:
                                try:
                                    remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(bunch_up[i])))
                                    cache_3, request_queue_3 = remote.andna.get_auth_cache()
                                    # succeded
                                    logging.debug('ANDNA: andna_hook: found a cache_3')
                                except Exception, e:
                                    logging.debug('ANDNA: andna_hook: getting cache from ' + \
                                        str(bunch_up[i]) + ' got exception ' + repr(e))
                            if cache_4 is None:
                                try:
                                    remote = TCPClient(ip_to_str(self.maproute.nip_to_ip(bunch_up[ANDNA_DUPLICATION+i])))
                                    cache_4, request_queue_4 = remote.andna.get_auth_cache()
                                    # succeded
                                    logging.debug('ANDNA: andna_hook: found a cache_4')
                                except Exception, e:
                                    logging.debug('ANDNA: andna_hook: getting cache from ' + \
                                        str(bunch_up[ANDNA_DUPLICATION+i]) + ' got exception ' + repr(e))
                        if cache_1 is None: cache_1 = []
                        if cache_2 is None: cache_2 = []
                        if cache_3 is None: cache_3 = []
                        if cache_4 is None: cache_4 = []
                        if request_queue_1 is None: request_queue_1 = []
                        if request_queue_2 is None: request_queue_2 = []
                        if request_queue_3 is None: request_queue_3 = []
                        if request_queue_4 is None: request_queue_4 = []
                        # We accept a record that is in 2 and in 3.
                        for hname in cache_2:
                            # Do I have already this record?
                            if hname not in self.cache:
                                # No. Is it in 3?
                                if hname in cache_3:
                                    # Yes. Memorize it.
                                    self.cache[hname] = cache_2[hname]
                                    logging.debug('ANDNA: andna_hook: got cache for ' + \
                                            str(hname))
                        for hname in request_queue_2:
                            # Do I have already this record?
                            if hname not in self.request_queue:
                                # No. Is it in 3?
                                if hname in request_queue_3:
                                    # Yes. Memorize it.
                                    self.request_queue[hname] = request_queue_2[hname]
                                    logging.debug('ANDNA: andna_hook: got request_queue for ' + \
                                            str(hname))
                        # We accept a record that is in 2 and not in 1.
                        for hname in cache_2:
                            # Do I have already this record?
                            if hname not in self.cache:
                                # No. Is it in 1?
                                if hname not in cache_1:
                                    # No. Memorize it.
                                    self.cache[hname] = cache_2[hname]
                                    logging.debug('ANDNA: andna_hook: got cache for ' + \
                                            str(hname))
                        for hname in request_queue_2:
                            # Do I have already this record?
                            if hname not in self.request_queue:
                                # No. Is it in 1?
                                if hname not in request_queue_1:
                                    # No. Memorize it.
                                    self.request_queue[hname] = request_queue_2[hname]
                                    logging.debug('ANDNA: andna_hook: got request_queue for ' + \
                                            str(hname))
                        # We accept a record that is in 3 and not in 4.
                        for hname in cache_3:
                            # Do I have already this record?
                            if hname not in self.cache:
                                # No. Is it in 1?
                                if hname not in cache_4:
                                    # No. Memorize it.
                                    self.cache[hname] = cache_3[hname]
                                    logging.debug('ANDNA: andna_hook: got cache for ' + \
                                            str(hname))
                        for hname in request_queue_3:
                            # Do I have already this record?
                            if hname not in self.request_queue:
                                # No. Is it in 1?
                                if hname not in request_queue_4:
                                    # No. Memorize it.
                                    self.request_queue[hname] = request_queue_3[hname]
                                    logging.debug('ANDNA: andna_hook: got request_queue for ' + \
                                            str(hname))

            self.events.send('ANDNA_HOOKED', ())
            self.wait_andna_hook = False
            logging.info('ANDNA: andna_hook: Emit signal ANDNA_HOOKED.')

            # We received COUNTER_HOOK, but the counter nodes could take longer
            # to know our nip and pubk. So wait a little, then I can register my names
            swait(3000)
            logging.debug('ANDNA: andna_hook: starting registration of my names.')
            self.register_my_names()

        finally:
            del self.micro_to_kill['andna_hook']

    @microfunc(True, keep_track=1)
    def register_my_names(self):
        ''' Try to register my names, and keep updating '''
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'register_my_names' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['register_my_names'])
        try:
            self.micro_to_kill['register_my_names'] = micro_current()

            # TODO: this way of obtaining my hostnames has to be reworked.
            import ntk.lib.misc as misc
            snsd_nodes = []
            try:
                hostname = misc.get_hostname()
                snsd_node = [True, hostname, 'me', 'NULL', 1, 1, None]
                snsd_nodes.append([TimeCapsule(0), snsd_node])
            except Exception, e:
                logging.debug('ANDNA: get_hostname got ' + repr(e))
            for line in misc.read_nodes(settings.SNSD_NODES_PATH):
                result, data = misc.parse_snsd_node(line)
                if not result:
                    raise ImproperlyConfigured("Wrong line in "+str(settings.SNSD_NODES_PATH))
                snsd_nodes.append([TimeCapsule(0), data])

            logging.debug('ANDNA: try to register the following SNSD records to myself: ' + str(snsd_nodes))

            while True:
                # Wait that there is something to do
                def something_to_do():
                    for pair in snsd_nodes:
                        capsule, snsd_node = pair
                        ttl = capsule.get_ttl()
                        if ttl <= 0:
                            return True
                    return False
                while_condition(something_to_do, wait_millisec=100)
                # Do [again] the requests.
                for pair in snsd_nodes:
                    capsule, snsd_node = pair
                    ttl = capsule.get_ttl()
                    if ttl <= 0:
                        try:
                            logging.debug('ANDNA: try to register ' + str(snsd_node))
                            append = snsd_node[0]
                            hostname = snsd_node[1]
                            record = snsd_node[2]
                            if record == 'me':
                                record = None
                            serv_key = make_serv_key(snsd_node[3])
                            priority = int(snsd_node[4])
                            weight = int(snsd_node[5])
                            pubk = None
                            pem_file = snsd_node[6]
                            if pem_file is not None:
                                pubk = PublicKey(from_file=pem_file)
                            snsd_record = SnsdAuthRecord(record, pubk, priority, weight)
                            ret = self.register(hostname, serv_key, 0, snsd_record, append)
                            logging.debug('ANDNA: Registration: trying to register ' + str(snsd_node) + ' got ' + str(ret))
                            res, msg = ret
                            if res == 'OK':
                                # the message is the ttl of the registration
                                ttl = int(msg * 0.8)
                            elif res == 'RETRY':
                                # the failure may be temporary
                                ttl = 1000 * 60 * 2 # 2 minutes in millisec
                            else:
                                # the failure should be quite stable
                                ttl = 1000 * 60 * 60 * 24 * 2 # 2 days in millisec
                        except Exception, e:
                            logging.error('ANDNA: Registration: trying to register ' + str(snsd_node) + ' got ' + repr(e))
                            # the failure may be temporary
                            ttl = 1000 * 60 * 2 # 2 minutes in millisec
                        logging.debug('ANDNA: Registration for ' + str(snsd_node) + ' will be ok for ' + str(ttl) + ' msec.')
                        pair[0] = TimeCapsule(ttl)

        finally:
            del self.micro_to_kill['register_my_names']

    ########## Helper functions used as a client of Andna, to send requests.

    def register(self, hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable):
        ''' Try registration or update of an hostname, record in tha ANDNA distributed database.
        '''
        logging.debug('ANDNA: register' + str((hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable)))

        res, msg = self.register_0(hostname, serv_key, IDNum, snsd_record, append_if_unavailable)
        if res == 'OK':
            logging.debug('ANDNA: register: main registration done.')
            logging.debug('ANDNA: register: starting registration spread.')
            for i in xrange(1, ANDNA_SPREAD):
                self.register_n(hostname, i)
        else:
            logging.debug('ANDNA: register: main registration error. ' + str((res, msg)))
        return res, msg

    def register_0(self, hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable):
        """ Register or update the name for the specified service number """
        logging.debug('ANDNA: register_0' + str((hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable)))

        # calculate hash
        hash_node = self.peer(key=hostname)
        logging.debug('ANDNA: register_0: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        sender_nip = self.maproute.me[:]
        # sign the request and attach the public key
        signature = self.my_keys.sign(rencode.dumps((sender_nip, hostname,
                serv_key, IDNum, snsd_record)))
        logging.debug('ANDNA: register_0: request registration')
        res, msg = hash_node.register_hostname_main(sender_nip,
                                              self.my_keys.get_pub_key(),
                                              hostname,
                                              serv_key,
                                              IDNum,
                                              snsd_record,
                                              signature,
                                              append_if_unavailable)
        if res == 'OK':
            self.local_cache[hostname] = msg

            logging.debug('ANDNA: register_0: registration done.')
            logging.debug('ANDNA: register_0: after register: self.local_cache=' + str(self.local_cache))
            logging.debug('ANDNA: register_0: after register: self.resolved=' + str(self.resolved))
        else:
            logging.debug('ANDNA: register_0: registration error. ' + str((res, msg)))
        return (res, msg)

    @microfunc(True, keep_track=1)
    def register_n(self, hostname, spread_number):
        """ Register or update the name for the specified service number """
        logging.debug('ANDNA: register_n' + str((hostname, spread_number)))

        # calculate hash
        hash_node = self.peer(key=(hostname, spread_number))
        logging.debug('ANDNA: register_n: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        logging.debug('ANDNA: register_n: request registration')
        ret = hash_node.register_hostname_spread(hostname, spread_number)
        logging.debug('ANDNA: register_n: returns ' + str(ret))

    def request_registrar_pubk(self, hostname):
        """ Request to hash-node the public key of the registrar of this hostname """
        logging.debug('ANDNA: request_registrar_pubk(' + str(hostname) + ')')
        # calculate hash
        hash_node = self.peer(key=hostname)
        logging.debug('ANDNA: request_registrar_pubk: exact hash_node is ' + str(hash_node.get_hash_nip()))
        # contact the hash node
        logging.debug('ANDNA: request_registrar_pubk: request registrar public key...')
        return hash_node.get_registrar_pubk(hostname)

    def check_expirations_resolved_cache(self):
        # Remove the expired entries from the resolved cache
        logging.debug('ANDNA: cleaning expired entries - if any - from our resolved cache...')
        logging.debug('ANDNA: self.resolved was ' + str(self.resolved))
        for key, resolved_record in self.resolved.items():
            if resolved_record.expires < time():
                self.resolved.pop(key)
                logging.debug('ANDNA: cleaned ' + str(key) + ' from resolved cache')
        logging.debug('ANDNA: self.resolved is ' + str(self.resolved))
        logging.debug('ANDNA: self.resolved_registrar_nip was ' + str(self.resolved_registrar_nip))
        for key, tupl in self.resolved_registrar_nip.items():
            nip, timecapsule_ttl = tupl
            if timecapsule_ttl.get_ttl() < 0:
                self.resolved_registrar_nip.pop(key)
                logging.debug('ANDNA: cleaned ' + str(key) + ' from resolved_registrar_nip cache')
        logging.debug('ANDNA: self.resolved_registrar_nip is ' + str(self.resolved_registrar_nip))

    def resolve(self, hostname, serv_key=NULL_SERV_KEY, no_chain=False):
        """ Resolve the hostname, returns the AndnaResolvedRecord associated to the hostname and service """
        logging.debug('ANDNA: resolve' + str((hostname, serv_key)))
        res, data = '', ''

        try:
            # Remove the expired entries from the resolved cache
            self.check_expirations_resolved_cache()

            # first try to resolve locally
            if self.resolved.has_key((hostname, serv_key)):
                data = self.resolved[(hostname, serv_key)]
                res = 'NOTFOUND' if data.records is None else 'OK'
                return res, data
            # else call the remote hash node
            choose_from = [(self.peer(key=hostname), 0)]
            for i in xrange(1, ANDNA_SPREAD):
                choose_from.append((self.peer(key=(hostname, i)), i))
            def get_nip(pair):
                peer, i = pair
                return self.H(peer.get_hash_nip())
            logging.debug('ANDNA: resolve: choose: ' + str(choose_from))
            hash_node, spread_number = self.maproute.choose_fast(choose_from, get_nip)
            logging.debug('ANDNA: resolve: exact hash_node is ' + str(hash_node.get_hash_nip()))
            # contact the hash gnode
            control, data = hash_node.reply_resolve(hostname, serv_key, no_chain, spread_number)
            if control == 'OK':
                self.resolved[(hostname, serv_key)] = data
                res = 'NOTFOUND' if data.records is None else 'OK'
            else:
                res = control
        except Exception, e:
            logging.debug('ANDNA: resolve: could not resolve right now:')
            log_exception_stacktrace(e)
            res, data = 'CANTRESOLVE', 'Could not resolve right now'
        return res, data

    def ask_registrar_nip(self, hostname):
        """ Asks the NIP of the registrar of hostname """
        logging.debug('ANDNA: ask_registrar_nip(' + str(hostname) + ')')
        try:
            # Remove the expired entries from the resolved cache
            self.check_expirations_resolved_cache()

            # first try to resolve locally
            if self.resolved_registrar_nip.has_key(hostname):
                nip, timecapsule_ttl = self.resolved_registrar_nip[hostname]
                return nip, timecapsule_ttl.get_ttl()
            # else call the remote hash node
            # calculate hash
            hash_node = self.peer(key=hostname)
            logging.debug('ANDNA: ask_registrar_nip: exact hash_node is ' + str(hash_node.get_hash_nip()))
            # contact the hash node
            data = hash_node.reply_registrar_nip(hostname)
            if data is not None:
                nip, ttl = data
                self.resolved_registrar_nip[hostname] = nip, TimeCapsule(ttl)
            return data
        except Exception, e:
            logging.debug('ANDNA: ask_registrar_nip: could not resolve right now:')
            log_exception_stacktrace(e)
            return None

    ########## Remotable and helper functions used as a server, to serve requests.

    def check_expirations_cache(self):
        # Remove the expired entries from the ANDNA cache
        logging.debug('ANDNA: cleaning expired entries - if any - from our ANDNA cache...')
        logging.debug('ANDNA: self.cache was ' + str(self.cache))
        for hname, auth_record in self.cache.items():
            if auth_record.expires < time():
                self.cache.pop(hname)
                logging.debug('ANDNA: cleaned ' + hname + ' from pubk ' + auth_record.pubk.short_repr())
                if self.request_queue.has_key(hname) and len(self.request_queue.has_key(hname)) > 0:
                    sender_nip, pubk, hostname, serv_key, IDNum, \
                       snsd_record, signature = self.request_queue[hname].pop(0)
                    logging.debug('ANDNA: trying to register it to pubk ' + pubk.short_repr())
                    # TODO to be tested
                    res, data = self.register_hostname_main(
                                sender_nip, pubk, hostname, serv_key, IDNum,
                                snsd_record, signature,
                                append_if_unavailable=False)
                    if res == 'OK':
                        # TODO make this passage in a tasklet
                        client = self.peer(hIP=sender_nip)
                        timestamp, updates = data
                        client.reply_queued_registration(hname, timestamp,
                                                         updates)
        logging.debug('ANDNA: self.cache is ' + str(self.cache))

    def register_hostname_main(self, sender_nip, pubk, hostname, serv_key, IDNum,
                       snsd_record, signature, append_if_unavailable,
                       forward=True):
      ''' Serves a request to register/update an hostname.
      '''
      start_tracking()
      try:
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a registration request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: register_hostname_main(' + str(sender_nip) + ', ' + str(hostname) + ', ...)')
        ret = '', ''

        if snsd_record.record is not None:
            if not isinstance(snsd_record.record, str):
                raise AndnaError, 'Record MUST be an hostname or None.'
            if len(snsd_record.record) > MAX_HOSTNAME_LEN:
                raise AndnaError, 'Record exceeded hostname maximum ' + \
                            'length (' + str(MAX_HOSTNAME_LEN) + ')'

        # Remove the expired entries from the ANDNA cache
        self.check_expirations_cache()

        # first the hash check
        logging.debug('ANDNA: register_hostname_main: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: register_hostname_main: exact hash_node is ' + str(hash_nip))

        if settings.ANDNA_REPLICA_ACTIVATED:
            # use ANDNA_DUPLICATION to check if maproute.me is in the bunch
            logging.debug('ANDNA: register_hostname_main: starting find_nearest ANDNA_DUPLICATION' + \
                        ' to ' + str(hash_nip))
            bunch = self.find_nearest_exec(hash_nip, ANDNA_DUPLICATION, \
                        self.maproute.levels)
            logging.debug('ANDNA: register_hostname_main: nearest ANDNA_DUPLICATION to ' + \
                        str(hash_nip) + ' is ' + str(bunch))
            if not any(bunch):
                raise AndnaError, 'Andna sees no participants.'
            check_hash = self.maproute.me in bunch
        else:
            check_hash = self.maproute.me == self.H(self.h(hostname))

        if not check_hash:
            logging.info('ANDNA: register_hostname_main: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: register_hostname_main: hash is verified.')

        # then the authentication check
        logging.debug('ANDNA: register_hostname_main: authenticating the request...')
        if not pubk.verify(rencode.dumps((sender_nip, hostname, serv_key, IDNum,
                       snsd_record)), signature):
            logging.info('ANDNA: register_hostname_main: request is NOT authenticated. Raising exception.')
            raise AndnaError, 'Request authentication failed'
        logging.debug('ANDNA: register_hostname_main: request authenticated')

        registered = False
        enqueued = False
        refused = False
        updated = False

        # If the snsd record points to a hostname, verify that
        # resolving the hostname we get the right public key.
        # TODO this has to have a counter-part in resolve.
        if isinstance(snsd_record.record, str):
            logging.debug('ANDNA: register_hostname_main: SNSD record points to another hostname: ' + snsd_record.record)
            logging.debug('ANDNA: register_hostname_main: Its public key should be: ' + snsd_record.pubk.short_repr())
            response, resolved_pubk = self.request_registrar_pubk(snsd_record.record)
            if response == 'OK':
                if resolved_pubk != snsd_record.pubk:
                    # reject this request
                    logging.debug('ANDNA: register_hostname_main: The pointed hostname is someone else. Its public key is: ' + resolved_pubk.short_repr())
                    ret = 'NOTVALID', 'The pointed hostname is someone else.'
                    refused = True
                else:
                    # proceed
                    logging.debug('ANDNA: register_hostname_main: The pointed hostname has that key. OK.')
            else:
                # the record has not been registered, so we cannot use it.
                # reject this request
                logging.debug('ANDNA: register_hostname_main: The pointed hostname cannot be resolved.')
                ret = 'RETRY', 'The pointed hostname cannot be resolved.'
                # This problem may be very temporary
                refused = True

        if not refused:
            # check if already registered
            if self.cache.has_key(hostname):
                logging.debug('ANDNA: register_hostname_main: record already in cache...')
                # check if the registrar is the same
                old_record = self.cache[hostname]
                old_registrar = old_record.pubk
                if old_registrar == pubk:
                    # This is an update.
                    logging.debug('ANDNA: register_hostname_main: ... this is an update')
                    # Check the record in Counter node
                    logging.debug('ANDNA: register_hostname_main: contacting counter gnode')
                    res, ttl = self.counter.ask_check(sender_nip, pubk, hostname)
                    if not res:
                        raise AndnaError('ANDNA: Failed counter check.')
                    # update record
                    old_record.store(nip=sender_nip, updates=IDNum, serv_key=serv_key, record=snsd_record)
                    ret = 'OK', old_record.get_ttl()
                    updated = True
                else:
                    logging.debug('ANDNA: register_hostname_main: ... and it is not from this pubk...')
                    # check if we want and can append
                    if append_if_unavailable:
                        if not self.request_queue.has_key(hostname):
                            self.request_queue[hostname] = []
                        if len(self.request_queue[hostname]) < MAX_ANDNA_QUEUE:
                            # append record
                            args_tuple_passed_to_register = (sender_nip,
                                    hostname, service, pubk, signature,
                                    snsd_record, snsd_record_pubk)
                            self.request_queue[hostname].append(args_tuple_passed_to_register)
                            logging.debug('ANDNA: register_hostname_main: ... request enqueued')
                            ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, request enqueued.'
                            enqueued = True
                        else:
                            logging.debug('ANDNA: register_hostname_main: ... queue is full')
                            ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue is full.'
                            refused = True
                    else:
                        logging.debug('ANDNA: register_hostname_main: ... queue not requested')
                        ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue not requested.'
                        refused = True

        if not (enqueued or refused or updated):
            # This is the first registration of an hostname
            # The first registration has to be necessarily a NULL_SERV_KEY
            # and the snsd record MUST be the sender_nip
            # TODO if we receive another type of request (eg forwarding still not complete)
            #      we might implicitly add a first request: we have all the data.
            if serv_key != NULL_SERV_KEY:
                logging.debug('ANDNA: register_hostname_main: The first registration has to be a Zero Service record')
                # reject this request
                ret = 'RETRY', 'The first registration has to be a Zero Service record.'
                # This problem may be very temporary, because of forwarding still not complete.
            elif snsd_record.record is not None:
                logging.debug('ANDNA: register_hostname_main: The first registration has to be the registrar\'s NIP')
                # reject this request
                ret = 'RETRY', 'The first registration has to be the registrar\'s NIP.'
                # This problem may be very temporary, because of forwarding still not complete.
            else:
                # Check the record in Counter node
                logging.debug('ANDNA: register_hostname_main: contacting counter gnode')
                res, ttl = self.counter.ask_check(sender_nip, pubk, hostname)
                if not res:
                    raise AndnaError, 'ANDNA: Failed counter check.'

                # register record
                logging.debug('ANDNA: register_hostname_main: registering ' + str(hostname))
                services = {}
                services[serv_key] = [snsd_record]
                new_record = AndnaAuthRecord(hostname, pubk, sender_nip, 0, 1, services)
                self.cache[hostname] = new_record
                registered = True
                ret = 'OK', new_record.get_ttl()

        if forward and (updated or registered):
            if settings.ANDNA_REPLICA_ACTIVATED:
                # forward the entry to the bunch
                bunch_not_me = [n for n in bunch if n != self.maproute.me]
                logging.debug('ANDNA: register_hostname_main: forward_registration to ' + str(bunch_not_me))
                self.forward_registration_main_to_set(bunch_not_me, \
                            (sender_nip, pubk,
                             hostname, serv_key, IDNum, snsd_record, signature,
                             append_if_unavailable))

        logging.debug('ANDNA: register_hostname_main: after register_hostname_main: self.request_queue=' + str(self.request_queue))
        logging.debug('ANDNA: register_hostname_main: after register_hostname_main: self.cache=' + str(self.cache))
        logging.debug('ANDNA: register_hostname_main: after register_hostname_main: self.resolved=' + str(self.resolved))
        logging.debug('ANDNA: register_hostname_main: register_hostname_main: returning ' + str(ret))
        return ret
      finally:
        stop_tracking()

    # only used if ANDNA_REPLICA_ACTIVATED
    @microfunc(True)
    def forward_registration_main_to_set(self, to_set, args_to_register_hostname_main):
        for to_nip in to_set:
            self.forward_registration_main_to_nip(to_nip, args_to_register_hostname_main)

    # only used if ANDNA_REPLICA_ACTIVATED
    @microfunc(True, keep_track=1)
    def forward_registration_main_to_nip(self, to_nip, args_to_register_hostname_main):
        """ Forwards registration request to another hash node in the bunch. """
        try:
            logging.debug('ANDNA: forwarding registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)))
            # TODO Use TCPClient or P2P ?
            remote = self.peer(hIP=to_nip)
            args_to_register_hostname_main = \
                    tuple(list(args_to_register_hostname_main) + [False])
            resp = remote.register_hostname_main(*args_to_register_hostname_main)
            logging.debug('ANDNA: forwarded registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)) + \
                    ' got ' + str(resp))
        except Exception, e:
            logging.warning('ANDNA: forwarded registration request to ' + \
                    str(to_nip) + \
                    ' got exception ' + repr(e))

    def register_hostname_spread(self, hostname, spread_number,
                       forward=True):
        ''' Serves a request to register/update an hostname, as
            one of the <n> "spread" server.
        '''
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a registration request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: register_hostname_spread' + str((hostname, spread_number)))

        # first the hash check
        logging.debug('ANDNA: register_hostname_spread: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=(hostname, spread_number))
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: register_hostname_spread: exact hash_node is ' + str(hash_nip))

        if settings.ANDNA_REPLICA_ACTIVATED:
            # use ANDNA_DUPLICATION to check if maproute.me is in the bunch
            logging.debug('ANDNA: register_hostname_spread: starting find_nearest ANDNA_DUPLICATION' + \
                        ' to ' + str(hash_nip))
            bunch = self.find_nearest_exec(hash_nip, ANDNA_DUPLICATION, \
                        self.maproute.levels)
            logging.debug('ANDNA: register_hostname_spread: nearest ANDNA_DUPLICATION to ' + \
                        str(hash_nip) + ' is ' + str(bunch))
            if not any(bunch):
                raise AndnaError, 'Andna sees no participants.'
            check_hash = self.maproute.me in bunch
        else:
            check_hash = self.maproute.me == self.H(self.h(hostname))

        if not check_hash:
            logging.info('ANDNA: register_hostname_spread: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: register_hostname_spread: hash is verified.')

        remote_main = self.peer(key=hostname)
        andnaauth = remote_main.get_auth_cache(hostname)
        if andnaauth is None:
            logging.debug('ANDNA: register_hostname_spread: hostname is not registered in main hash node.')
            # TODO better an Exception?
            return False
        self.cache[hostname] = andnaauth
        logging.debug('ANDNA: register_hostname_spread: hostname registered.')

        if forward:
            if settings.ANDNA_REPLICA_ACTIVATED:
                # forward the entry to the bunch
                bunch_not_me = [n for n in bunch if n != self.maproute.me]
                logging.debug('ANDNA: register_hostname_spread: forward_registration to ' + str(bunch_not_me))
                self.forward_registration_spread_to_set(bunch_not_me, \
                            (hostname, spread_number))

        return True

    # only used if ANDNA_REPLICA_ACTIVATED
    @microfunc(True)
    def forward_registration_spread_to_set(self, to_set, args_to_register_hostname_spread):
        for to_nip in to_set:
            self.forward_registration_spread_to_nip(to_nip, args_to_register_hostname_spread)

    # only used if ANDNA_REPLICA_ACTIVATED
    @microfunc(True, keep_track=1)
    def forward_registration_spread_to_nip(self, to_nip, args_to_register_hostname_spread):
        """ Forwards registration (spread) request to another hash node in the bunch. """
        try:
            logging.debug('ANDNA: forwarding registration (spread) request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)))
            # TODO Use TCPClient or P2P ?
            remote = self.peer(hIP=to_nip)
            args_to_register_hostname_spread = \
                    tuple(list(args_to_register_hostname_spread) + [False])
            resp = remote.register_hostname_spread(*args_to_register_hostname_spread)
            logging.debug('ANDNA: forwarded registration (spread) request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)) + \
                    ' got ' + str(resp))
        except Exception, e:
            logging.warning('ANDNA: forwarded registration (spread) request to ' + \
                    str(to_nip) + \
                    ' got exception ' + repr(e))

    def reply_queued_registration(self, hostname, timestamp, updates):
        """ The registration request we have sent and enqueued 
        when the hostname was busy has been satisfied now """
        # Maybe, there is no need for this function. The method that
        #  has the duty to keep the registration refreshed (that is,
        #  register_my_names) will have the memo. Isn't it?
        # TODO: should I print a message to the user?
        self.local_cache[hostname] = timestamp, updates
        return ('OK', (timestamp, updates))

    def get_registrar_pubk(self, hostname, spread_number=0):
        """ Return the public key of the registrar of this hostname """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a resolution request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: get_registrar_pubk(' + str(hostname) + ')')
        # first the hash check
        logging.debug('ANDNA: get_registrar_pubk: verifying that I am the right hash...')
        check_hash = self.maproute.me == self.H(self.h((hostname, spread_number)))

        if not check_hash:
            logging.info('ANDNA: get_registrar_pubk: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: get_registrar_pubk: hash is verified.')

        if hostname in self.cache:
            andna_cache = self.cache[hostname]
            ret = ('OK', andna_cache.pubk)
        else:
            ret = ('NOTFOUND', 'Hostname not registered.')
        logging.debug('ANDNA: get_registrar_pubk: returning ' + str(ret))
        return ret

    def reply_resolve(self, hostname, serv_key, no_chain=False, spread_number=0):
        """ Return the AndnaResolvedRecord associated to the hostname and service """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a resolution request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: reply_resolve' + str((hostname, serv_key, no_chain, spread_number)))
        # first the hash check
        logging.debug('ANDNA: reply_resolve: verifying that I am the right hash...')
        check_hash = self.maproute.me == self.H(self.h((hostname, spread_number)))

        if not check_hash:
            logging.info('ANDNA: reply_resolve: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: reply_resolve: hash is verified.')

        # Remove the expired entries from the ANDNA cache
        self.check_expirations_cache()

        data = self.get_resolved_record(hostname, serv_key)
        logging.debug('ANDNA: reply_resolve: get_resolved_record returns ' + str(data))

        if data.records is not None:
            for i in xrange(len(data.records)):
                datarec = data.records[i]
                if isinstance(datarec.record, str):
                    if no_chain:
                        logging.debug('ANDNA: reply_resolve: no_chain: will discard ' + datarec.record + '.')
                        data.records[i] = None
                    else:
                        # Resolve snsd names for the client
                        logging.debug('ANDNA: reply_resolve: resolving ' + datarec.record + ' on behalf of our client.')
                        ris, ris_rec = self.resolve(datarec.record, no_chain=True)
                        logging.debug('ANDNA: reply_resolve: ' + datarec.record + ' resolved as ' + str((ris, ris_rec)))
                        if ris == 'OK' and \
                                len(ris_rec.records) > 0 and \
                                isinstance(ris_rec.records[0].record, list):
                            data.records[i].record = ris_rec.records[0].record
                        else:
                            data.records[i] = None
            while None in data.records:
                data.records.remove(None)

        logging.debug('ANDNA: reply_resolve: returning ' + str(data))
        return 'OK', data

    def reply_registrar_nip(self, hostname, spread_number=0):
        """ Return the NIP of the registrar of hostname and its TTL """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a resolution request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: reply_registrar_nip(' + str(hostname) + ')')
        # first the hash check
        logging.debug('ANDNA: reply_registrar_nip: verifying that I am the right hash...')
        check_hash = self.maproute.me == self.H(self.h((hostname, spread_number)))

        if not check_hash:
            logging.info('ANDNA: reply_registrar_nip: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: reply_registrar_nip: hash is verified.')

        # Remove the expired entries from the ANDNA cache
        self.check_expirations_cache()

        data = None
        if hostname in self.cache:
            data = self.cache[hostname].nip[:], self.cache[hostname].get_ttl()
        logging.debug('ANDNA: reply_registrar_nip: returns ' + str(data))
        return data

    def get_resolved_record(self, hostname, serv_key):
        ''' Helper method to retrieve from the ANDNA cache an AndnaAuthRecord
            and create a AndnaResolvedRecord
        '''
        if hostname in self.cache:
            return self.cache[hostname].get_resolved_record(serv_key)
        else:
            return AndnaResolvedRecord(MAX_TTL_OF_NEGATIVE, None)

    def get_auth_cache(self, key=None):
        ''' Returns the cache of authoritative records.
        '''
        if self.wait_andna_hook:
            raise AndnaExpectableException, 'Andna is hooking. Request not valid.'
        if key is None:
            return self.cache, self.request_queue
        else:
            # The client wants only this key = hostname
            # The answer is an element of self.cache.
            if key in self.cache:
                return self.cache[key]
            else:
                return None

    def h(self, key):
        """ Retrieve an IP from the hostname """
        if isinstance(key, tuple):
            hostname, spread_number = key
        else:
            hostname, spread_number = key, 0
        return hash_32bit_ip(md5(hostname + str(spread_number)), self.maproute.levels, 
                             self.maproute.gsize)

def hash_32bit_ip(hashed, levels, gsize):
    digest = fnv_32_buf(md5(hashed))
    nip = [None] * levels
    for i in reversed(xrange(levels)): 
        nip[i] = int(digest % gsize)
        digest = int(digest / gsize)
    return nip

