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

        # hostnames that I tried to register to myself
        self.wanted_hostnames = []
    
        self.remotable_funcs += [self.reply_register,
                                 self.confirm_your_request,
                                 self.reply_resolve,
                                 self.get_registrar_pubk,
                                 self.reply_queued_registration,
                                 self.get_auth_cache]

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
            logging.debug('ANDNA: resetted.')

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
                logging.debug('ANDNA: getting cache from ' + str(cache_nip))
                try:
                    cache_from_nip, req_queue_from_nip = remote.andna.get_auth_cache()
                    for hname in cache_from_nip.keys():
                        # TODO perhaps we don't have this key, but we already
                        #      tried and we refused because we're not in the
                        #      bunch. In this case we should avoid to start a
                        #      find_nearest again.
                        # Do I have already this record?
                        if hname not in self.cache:
                            # No. Do I am the right hash for this record?
                            hname_nip = self.peer(key=hname).get_hash_nip()

                            # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
                            hname_bunch = [hname_nip]

                            if self.maproute.me in hname_bunch:
                                # Yes. Memorize it.
                                self.cache[hname] = cache_from_nip[hname]
                                if hname in req_queue_from_nip:
                                    self.request_queue[hname] = req_queue_from_nip[hname]
                                logging.debug('ANDNA: got cache for ' + \
                                        str(hname))
                    logging.debug('ANDNA: executed cache from ' + \
                            str(cache_nip))
                except Exception, e:
                    logging.debug('ANDNA: getting cache from ' + \
                            str(cache_nip) + ' got exception ' + repr(e))

            self.events.send('ANDNA_HOOKED', ())
            self.wait_andna_hook = False
            logging.info('Andna: Emit signal ANDNA_HOOKED.')

            # We received COUNTER_HOOK, so the counter nodes
            # should know our nip and pubk. So I can register my names
            logging.debug('ANDNA: starting registration of my names.')
            self.register_my_names()

        finally:
            del self.micro_to_kill['andna_hook']

    @microfunc(True, keep_track=1)
    def register_my_names(self):
        ''' Try to register my names, and keep updating '''
        # The tasklet started with this function can be killed.
        # Furthermore, if it is called while it was already running in another
        # tasklet, it restarts itself.
        while 'andna_hook' in self.micro_to_kill:
            micro_kill(self.micro_to_kill['register_my_names'])
        try:
            self.micro_to_kill['register_my_names'] = micro_current()

            # TODO: this way of obtaining my hostnames has to be reworked.
            import ntk.lib.misc as misc
            from ntk.config import settings
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

    ########## remotable methods directly called to a NIP

    def confirm_your_request(self, nip, pubk):
        # This P2P-remotable method is called directyly to a certain NIP in order
        # to verify that he is the sender of a certain request. The method asks to
        # confirm that the NIP is the right one, that the public key is of this node.
        return nip == self.maproute.me and \
               pubk == self.my_keys.get_pub_key()

    ########## Helper functions used as a client of Andna, to send requests.

    def register(self, hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable):
        """ Register or update the name for the specified service number """
        logging.debug('ANDNA: register' + str((hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable)))
        if hostname not in self.wanted_hostnames:
            self.wanted_hostnames.append(hostname)

        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: register: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of BALANCING nodes
        bunch = [hash_nip]

        # TODO uncomment:
        ### If I am in the bunch, use myself
        ##if self.maproute.me in bunch:
        ##    random_hnode = self.maproute.me[:]
        ##else:
        if True:
            random_hnode = choice(bunch)
        logging.debug('ANDNA: register: random hash_node is ' + str(random_hnode))
        # contact the hash node
        hash_gnode = self.peer(hIP=random_hnode)
        sender_nip = self.maproute.me[:]
        # sign the request and attach the public key
        signature = self.my_keys.sign(rencode.dumps((sender_nip, hostname,
                serv_key, IDNum, snsd_record)))
        logging.debug('ANDNA: register: request registration')
        res, msg = hash_gnode.reply_register(sender_nip,
                                              self.my_keys.get_pub_key(),
                                              hostname,
                                              serv_key,
                                              IDNum,
                                              snsd_record,
                                              signature,
                                              append_if_unavailable)
        if res == 'OK':
            self.local_cache[hostname] = msg

            logging.debug('ANDNA: register: registration done.')
            logging.debug('ANDNA: register: after register: self.local_cache=' + str(self.local_cache))
            logging.debug('ANDNA: register: after register: self.resolved=' + str(self.resolved))
        else:
            logging.debug('ANDNA: register: registration error. ' + str((res, msg)))
        return (res, msg)

    def request_registrar_pubk(self, hostname):
        """ Request to hash-node the public key of the registrar of this hostname """
        logging.debug('ANDNA: request_registrar_pubk(' + str(hostname) + ')')
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: request_registrar_pubk: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of BALANCING nodes
        bunch = [hash_nip]

        # TODO uncomment:
        ### If I am in the bunch, use myself
        ##if self.maproute.me in bunch:
        ##    random_hnode = self.maproute.me[:]
        ##else:
        if True:
            random_hnode = choice(bunch)
        logging.debug('ANDNA: request_registrar_pubk: random hash_node is ' + str(random_hnode))
        # contact the hash node
        hash_gnode = self.peer(hIP=random_hnode)
        logging.debug('ANDNA: request_registrar_pubk: request registrar public key...')
        return hash_gnode.get_registrar_pubk(hostname)

    def check_expirations_resolved_cache(self):
        # Remove the expired entries from the resolved cache
        logging.debug('ANDNA: cleaning expired entries - if any - from our resolved cache...')
        for key, resolved_record in self.resolved.items():
            if resolved_record.expires < time():
                self.resolved.pop(key)
                logging.debug('ANDNA: cleaned ' + str(key) + ' from resolved cache')

    def resolve(self, hostname, serv_key=NULL_SERV_KEY):
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
            # calculate hash
            hash_node = self.peer(key=hostname)
            hash_nip = hash_node.get_hash_nip()
            logging.debug('ANDNA: resolve: exact hash_node is ' + str(hash_nip))

            # TODO find a mechanism to find a 'bunch' of BALANCING nodes
            bunch = [hash_nip]

            # TODO uncomment:
            ### If I am in the bunch, use myself
            ##if self.maproute.me in bunch:
            ##    random_hnode = self.maproute.me[:]
            ##else:
            if True:
                random_hnode = choice(bunch)
            logging.debug('ANDNA: resolve: random hash_node is ' + str(random_hnode))
            # contact the hash gnode
            hash_gnode = self.peer(hIP=random_hnode)
            control, data = hash_gnode.reply_resolve(hostname, serv_key)
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

    ########## Remotable and helper functions used as a server, to serve requests.

    def check_expirations_cache(self):
        # Remove the expired entries from the ANDNA cache
        logging.debug('ANDNA: cleaning expired entries - if any - from our ANDNA cache...')
        for hname, auth_record in self.cache.items():
            if auth_record.expires < time():
                self.cache.pop(hname)
                logging.debug('ANDNA: cleaned ' + hname + ' from pubk ' + auth_record.pubk.short_repr())
                if self.request_queue.has_key(hname) and len(self.request_queue.has_key(hname)) > 0:
                    sender_nip, pubk, hostname, serv_key, IDNum, \
                       snsd_record, signature = self.request_queue[hname].pop(0)
                    logging.debug('ANDNA: trying to register it to pubk ' + pubk.short_repr())
                    # TODO to be tested
                    res, data = self.reply_register(
                                sender_nip, pubk, hostname, serv_key, IDNum,
                                snsd_record, signature,
                                append_if_unavailable=False)
                    if res == 'OK':
                        # TODO make this passage in a tasklet
                        client = self.peer(hIP=sender_nip)
                        timestamp, updates = data
                        client.reply_queued_registration(hname, timestamp,
                                                         updates)

    def reply_register(self, sender_nip, pubk, hostname, serv_key, IDNum,
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

        logging.debug('ANDNA: reply_register(' + str(sender_nip) + ', ' + str(hostname) + ', ...)')
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
        logging.debug('ANDNA: reply_register: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: reply_register: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
        bunch = [hash_nip, self.maproute.me]

        check_hash = self.maproute.me in bunch

        if not check_hash:
            logging.info('ANDNA: reply_register: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: reply_register: hash is verified.')

        # then the authentication check
        logging.debug('ANDNA: reply_register: authenticating the request...')
        if not pubk.verify(rencode.dumps((sender_nip, hostname, serv_key, IDNum,
                       snsd_record)), signature):
            logging.info('ANDNA: reply_register: request is NOT authenticated. Raising exception.')
            raise AndnaError, 'Request authentication failed'
        logging.debug('ANDNA: reply_register: request authenticated')

        registered = False
        enqueued = False
        refused = False
        updated = False

        # If the snsd record points to a hostname, verify that
        # resolving the hostname we get the right public key.
        # TODO this has to have a counter-part in resolve.
        if isinstance(snsd_record.record, str):
            logging.debug('ANDNA: reply_register: SNSD record points to another hostname: ' + snsd_record.record)
            logging.debug('ANDNA: reply_register: Its public key should be: ' + snsd_record.pubk.short_repr())
            response, resolved_pubk = self.request_registrar_pubk(snsd_record.record)
            if response == 'OK':
                if resolved_pubk != snsd_record.pubk:
                    # reject this request
                    logging.debug('ANDNA: reply_register: The pointed hostname is someone else. Its public key is: ' + resolved_pubk.short_repr())
                    ret = 'NOTVALID', 'The pointed hostname is someone else.'
                    refused = True
                else:
                    # proceed
                    logging.debug('ANDNA: reply_register: The pointed hostname has that key. OK.')
            else:
                # the record has not been registered, so we cannot use it.
                # reject this request
                logging.debug('ANDNA: reply_register: The pointed hostname cannot be resolved.')
                ret = 'RETRY', 'The pointed hostname cannot be resolved.'
                # This problem may be very temporary
                refused = True

        if not refused:
            # check if already registered
            if self.cache.has_key(hostname):
                logging.debug('ANDNA: reply_register: record already in cache...')
                # check if the registrar is the same
                old_record = self.cache[hostname]
                old_registrar = old_record.pubk
                if old_registrar == pubk:
                    # This is an update.
                    logging.debug('ANDNA: reply_register: ... this is an update')
                    # Check the record in Counter node
                    logging.debug('ANDNA: reply_register: contacting counter gnode')
                    res, ttl = self.counter.ask_check(sender_nip, pubk, hostname)
                    if not res:
                        raise AndnaError('ANDNA: Failed counter check.')
                    # update record
                    old_record.store(nip=sender_nip, updates=IDNum, serv_key=serv_key, record=snsd_record)
                    ret = 'OK', old_record.get_ttl()
                    updated = True
                else:
                    logging.debug('ANDNA: reply_register: ... and it is not from this pubk...')
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
                            logging.debug('ANDNA: reply_register: ... request enqueued')
                            ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, request enqueued.'
                            enqueued = True
                        else:
                            logging.debug('ANDNA: reply_register: ... queue is full')
                            ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue is full.'
                            refused = True
                    else:
                        logging.debug('ANDNA: reply_register: ... queue not requested')
                        ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue not requested.'
                        refused = True

        if not (enqueued or refused or updated):
            # This is the first registration of an hostname
            # The first registration has to be necessarily a NULL_SERV_KEY
            # and the snsd record MUST be the sender_nip
            # TODO if we receive another type of request (eg forwarding still not complete)
            #      we might implicitly add a first request: we have all the data.
            if serv_key != NULL_SERV_KEY:
                logging.debug('ANDNA: reply_register: The first registration has to be a Zero Service record')
                # reject this request
                ret = 'RETRY', 'The first registration has to be a Zero Service record.'
                # This problem may be very temporary, because of forwarding still not complete.
            elif snsd_record.record is not None:
                logging.debug('ANDNA: reply_register: The first registration has to be the registrar\'s NIP')
                # reject this request
                ret = 'RETRY', 'The first registration has to be the registrar\'s NIP.'
                # This problem may be very temporary, because of forwarding still not complete.
            else:
                # Check the record in Counter node
                logging.debug('ANDNA: reply_register: contacting counter gnode')
                res, ttl = self.counter.ask_check(sender_nip, pubk, hostname)
                if not res:
                    raise AndnaError, 'ANDNA: Failed counter check.'

                # register record
                logging.debug('ANDNA: reply_register: registering ' + str(hostname))
                services = {}
                services[serv_key] = [snsd_record]
                new_record = AndnaAuthRecord(hostname, pubk, sender_nip, 0, 1, services)
                self.cache[hostname] = new_record
                registered = True
                ret = 'OK', new_record.get_ttl()

        if forward and (updated or registered):
            # forward the entry to the bunch
            bunch_not_me = [n for n in bunch if n != self.maproute.me]
            logging.debug('ANDNA: reply_register: forward_registration to ' + str(bunch_not_me))
            self.forward_registration_to_set(bunch_not_me, \
                        (sender_nip, pubk,
                         hostname, serv_key, IDNum, snsd_record, signature,
                         append_if_unavailable))

        logging.debug('ANDNA: reply_register: after reply_register: self.request_queue=' + str(self.request_queue))
        logging.debug('ANDNA: reply_register: after reply_register: self.cache=' + str(self.cache))
        logging.debug('ANDNA: reply_register: after reply_register: self.resolved=' + str(self.resolved))
        logging.debug('ANDNA: reply_register: reply_register: returning ' + str(ret))
        return ret
      finally:
        stop_tracking()

    @microfunc(True)
    def forward_registration_to_set(self, to_set, args_to_reply_register):
        for to_nip in to_set:
            self.forward_registration_to_nip(to_nip, args_to_reply_register)

    @microfunc(True, keep_track=1)
    def forward_registration_to_nip(self, to_nip, args_to_reply_register):
        """ Forwards registration request to another hash node in the bunch. """
        try:
            logging.debug('ANDNA: forwarding registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)))
            # TODO Use TCPClient or P2P ?
            remote = self.peer(hIP=to_nip)
            args_to_reply_register = \
                    tuple(list(args_to_reply_register) + [False])
            resp = remote.reply_register(*args_to_reply_register)
            logging.debug('ANDNA: forwarded registration request to ' + \
                    ip_to_str(self.maproute.nip_to_ip(to_nip)) + \
                    ' got ' + str(resp))
        except Exception, e:
            logging.warning('ANDNA: forwarded registration request to ' + \
                    str(to_nip) + \
                    ' got exception ' + repr(e))

    def reply_queued_registration(self, hostname, timestamp, updates):
        """ The registration request we have sent and enqueued 
        when the hostname was busy has been satisfied now """
        # TODO: should I print a message to the user?
        self.local_cache[hostname] = timestamp, updates
        return ('OK', (timestamp, updates))

    def get_registrar_pubk(self, hostname):
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
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: get_registrar_pubk: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
        bunch = [hash_nip, self.maproute.me]

        check_hash = self.maproute.me in bunch

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

    def reply_resolve(self, hostname, serv_key):
        """ Return the AndnaResolvedRecord associated to the hostname and service """
        # If we recently changed our NIP (we hooked) we wait to finish
        # an andna_hook before answering a resolution request.
        def exit_func():
            return not self.wait_andna_hook
        logging.debug('ANDNA: waiting andna_hook...')
        while_condition(exit_func, wait_millisec=1)
        logging.debug('ANDNA: We are correctly hooked.')
        # We are correctly hooked.

        logging.debug('ANDNA: reply_resolve' + str((hostname, serv_key)))
        # first the hash check
        logging.debug('ANDNA: reply_resolve: verifying that I am the right hash...')
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: reply_resolve: exact hash_node is ' + str(hash_nip))

        # TODO find a mechanism to find a 'bunch' of DUPLICATION nodes
        bunch = [hash_nip, self.maproute.me]

        check_hash = self.maproute.me in bunch

        if not check_hash:
            logging.info('ANDNA: reply_resolve: hash is NOT verified. Raising exception.')
            raise AndnaError, 'Hash verification failed'
        logging.debug('ANDNA: reply_resolve: hash is verified.')

        # Remove the expired entries from the ANDNA cache
        self.check_expirations_cache()

        data = self.get_resolved_record(hostname, serv_key)
        return 'OK', data

    def get_resolved_record(self, hostname, serv_key):
        ''' Helper method to retrieve from the ANDNA cache an AndnaAuthRecord
            and create a AndnaResolvedRecord
        '''
        if hostname in self.cache:
            return self.cache[hostname].get_resolved_record(serv_key)
        else:
            return AndnaResolvedRecord(MAX_TTL_OF_NEGATIVE, None)

    def get_auth_cache(self):
        ''' Returns the cache of authoritative records.
        '''
        if self.wait_andna_hook:
            raise AndnaExpectableException, 'Andna is hooking. Request not valid.'
        return self.cache, self.request_queue

    def h(self, hostname):
        """ Retrieve an IP from the hostname """    
        return hash_32bit_ip(md5(hostname), self.maproute.levels, 
                             self.maproute.gsize)

def hash_32bit_ip(hashed, levels, gsize):
    digest = fnv_32_buf(md5(hashed))
    mask = 0xff000000
    ip = [None] * levels
    for i in reversed(xrange(levels)): 
        ip[i] = int((digest & mask) % (gsize-1)) + 1
        mask >>= 8
    return ip
