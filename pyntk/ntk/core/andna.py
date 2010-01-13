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

from random import randint
import ntk.lib.rencode as rencode

from ntk.core.p2p import OptionalP2P
from ntk.core.snsd import SnsdRecord, MAX_TTL_OF_NEGATIVE, AndnaAuthRecord, AndnaResolvedRecord
from ntk.lib.crypto import md5, fnv_32_buf, verify, public_key_from_pem
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.log import log_exception_stacktrace
from ntk.lib.micro import microfunc
from ntk.lib.misc import is_ip
from ntk.lib.rencode import serializable
from ntk.network.inet import ip_to_str, str_to_ip
from ntk.wrap.xtime import timestamp_to_data, today, days, while_condition, time

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

# Used to calculate the range of authoritative sources for a given hostname
#ANDNA_DUPLICATION = 48
ANDNA_DUPLICATION = 1
#ANDNA_BALANCING = 12
ANDNA_BALANCING = 1

class AndnaError(Exception): pass

class Andna(OptionalP2P):
    
    pid = 3
    
    def __init__(self, ntkd_status, keypair, counter, radar, maproute, p2pall):
        OptionalP2P.__init__(self, ntkd_status, radar, maproute, Andna.pid)
        
        self.p2pall = p2pall
        
        # let's register ourself in p2pall
        self.p2pall.p2p_register(self)
        self.p2pall.events.listen('P2P_HOOKED', self.andna_hook)

        self.events = Event(['ANDNA_HOOKED'])

        # keep in sync the range to be contacted to register/resolve
        self.mapp2p.events.listen('NODE_NEW', self.calc_ranges)
        self.mapp2p.events.listen('NODE_DELETED', self.calc_ranges)

        self.max_andna_queue = 5
        
        self.counter = counter
        self.my_keys = keypair
        # Appended requests = { hostname: 
        #                       args_tuple_passed_to_register, ... }
        self.request_queue = {}
        # Andna Cache = { hostname: AndnaAuthRecord(), ... }
        self.cache = {} 
        # Local Cache = { hostname: (expires, updates), ... }
        self.local_cache = {}
        # Resolved Cache = { (hostname, serv_key): AndnaResolvedRecord(), ... }
        self.resolved = {}
    
        self.remotable_funcs += [self.reply_register, self.reply_resolve,
                                 self.reply_reverse_resolve, 
                                 self.reply_queued_registration,
                                 self.reply_resolved_cache,
                                 self.reply_forward_registration,
                                 self.cache_getall]

    def calc_min_lvl_num(self, node_numb):
        # TODO to be reviewed. Calculates how many g-nodes of which level
        #  we have to fully consider to have a minimum of node_numb nodes
        #  that exist and participate in the P2P service.
        from math import ceil
        prev = nodes = 1
        lvl = -1
        while nodes < node_numb and lvl+1 < self.mapp2p.levels:
            prev = nodes
            lvl += 1
            nodes = self.mapp2p.nodes_nb[lvl] * prev
        if nodes < node_numb:
            num = self.mapp2p.nodes_nb[lvl]
        else:
            num = int(ceil(float(node_numb)/float(prev)))
        return lvl, num

    def calc_ranges(self, *args):
        # TODO to be reviewed. Calculates how many g-nodes of which level
        #  we have to fully consider to have the required range of nodes
        #  to be contacted to register/resolve a hostname.
        self.duplication_lvl, self.duplication_num = \
                    self.calc_min_lvl_num(ANDNA_DUPLICATION)
        self.balancing_lvl, self.balancing_num = \
                    self.calc_min_lvl_num(ANDNA_BALANCING)

    def get_resolved_record(hostname, serv_key):
        if hostname in self.cache:
            return self.cache[hostname].get_resolved_record(serv_key)
        else:
            return AndnaResolvedRecord(hostname, serv_key, None, MAX_TTL_OF_NEGATIVE, None)

    def register_my_names(self):
        # register my names

        # TODO: this way of obtaining my hostnames has to be reworked.
        import ntk.lib.misc as misc
        from ntk.config import settings
        snsd_nodes = []
        for line in misc.read_nodes(settings.SNSD_NODES_PATH):
            result, data = misc.parse_snsd_node(line)
            if not result:
                raise ImproperlyConfigured("Wrong line in "+str(settings.SNSD_NODES_PATH))
            snsd_nodes.append(data)

        logging.debug('ANDNA: try to register the following SNSD records to myself: ' + str(snsd_nodes))
        for snsd_node in snsd_nodes:
            append = snsd_node[0]
            hostname = snsd_node[1]
            record = snsd_node[2]
            if record == 'me':
                record = self.maproute.me[:]
            serv_key = make_serv_key(snsd_node[3])
            priority = int(snsd_node[4])
            weight = int(snsd_node[5])
            pubk = None
            pem = snsd_node[6]
            if pem is not None:
                pubk = public_key_from_pem(pem)
            snsd_record = SnsdRecord(record, pubk, priority, weight)
            self.register(hostname, serv_key, 0, snsd_record, append)
            #def register(self, hostname, serv_key, IDNum, snsd_record,
            #    append_if_unavailable):

    @microfunc(True)
    def andna_hook(self):
        # clear old cache
        self.reset()
        logging.debug('ANDNA: resetted.')
        logging.debug('ANDNA: after reset: self.request_queue=' + str(self.request_queue))
        logging.debug('ANDNA: after reset: self.cache=' + str(self.cache))
        logging.debug('ANDNA: after reset: self.local_cache=' + str(self.local_cache))
        logging.debug('ANDNA: after reset: self.resolved=' + str(self.resolved))

        # register my names
        #self.register_my_names()

        # # merge ??
        # neigh = None
        # def no_participants():
        #     #TODO: name should be any_participant.
        #     #TODO: the number of valid ids in level levels-1 could be 1.
        #     #      We have to check in another way.
        #     return self.mapp2p.node_nb[self.mapp2p.levels-1] >= 1
        # # wait at least one participant
        # while_condition(no_participants)
        # for neigh in self.neigh.neigh_list(in_my_network=True):
        #     nip = self.maproute.ip_to_nip(neigh.ip)
        #     peer = self.peer(hIP=nip)
        #     self.caches_merge(peer.cache_getall())
        # self.events.send('ANDNA_HOOKED', ())

    def reset(self):
        self.request_queue = {}
        self.cache = {}
        self.local_cache = {}
        self.resolved = {}

    def register(self, hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable):
        """ Register or update the name for the specified service number """
        logging.debug('ANDNA: register' + str((hostname, serv_key, IDNum, snsd_record,
                append_if_unavailable)))
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: exact hash_node is ' + str(hash_nip))
        random_hnode = hash_nip[:]
        # NAAAH   random_hnode[0] = randint(0, self.maproute.gsize)
        # TODO use ANDNA_BALANCING to get a random node from a bunch
        logging.debug('ANDNA: random hash_node is ' + str(random_hnode))
        # contact the hash gnode firstly
        hash_gnode = self.peer(hIP=random_hnode)
        sender_nip = self.maproute.me[:]
        # sign the request and attach the public key
        signature = self.my_keys.sign(rencode.dumps((sender_nip, hostname,
                serv_key, IDNum, snsd_record)))
        logging.debug('ANDNA: request registration')
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

            logging.debug('ANDNA: registration done.')
            logging.debug('ANDNA: after register: self.my_keys=' + str(self.my_keys))
            logging.debug('ANDNA: after register: self.request_queue=' + str(self.request_queue))
            logging.debug('ANDNA: after register: self.cache=' + str(self.cache))
            logging.debug('ANDNA: after register: self.local_cache=' + str(self.local_cache))
            logging.debug('ANDNA: after register: self.resolved=' + str(self.resolved))
        else:
            logging.debug('ANDNA: registration error. ' + str((res, msg)))
        return (res, msg)

    def check_expirations(self):
        # Remove the expired entries from the ANDNA cache
        logging.debug('ANDNA: cleaning expired entries - if any - from our ANDNA cache...')
        for hname, auth_record in self.cache.items():
            if auth_record.expires < time():
                self.cache.pop(hname)
                logging.debug('ANDNA: cleaned ' + hname + ' from pubk ' + auth_record.pubk.short_repr())
                if self.request_queue.has_key(hname):
                    sender_nip, pubk, hostname, serv_key, IDNum, \
                       snsd_record, signature = self.request_queue[hname]
                    logging.debug('ANDNA: trying to register it to pubk ' + pubk.short_repr())
                    res, data = self.reply_register(
                                sender_nip, pubk, hostname, serv_key, IDNum,
                                snsd_record, signature,
                                append_if_unavailable=False)
                    if res == 'OK':
                        client = self.peer(hIP=sender_nip)
                        timestamp, updates = data
                        client.reply_queued_registration(hname, timestamp, 
                                                         updates)

    def reply_register(self, sender_nip, pubk, hostname, serv_key, IDNum,
                       snsd_record, signature, append_if_unavailable):

        ret = '', ''

        # Remove the expired entries from the ANDNA cache
        self.check_expirations()

        registration_time = updates = 0

        # first the hash check
        logging.debug('ANDNA: verifying that I am the right hash...')
        # TODO

        # then the authentication check
        logging.debug('ANDNA: authenticating the request...')
        if not verify(rencode.dumps((sender_nip, hostname, serv_key, IDNum,
                       snsd_record)), signature, pubk):
            raise AndnaError('Request authentication failed')
        logging.debug('ANDNA: request authenticated')

        # If the snsd record points to a hostname, verify that
        # resolving the hostname we get the right public key.
        if isinstance(snsd_record.record, str):
            logging.debug('ANDNA: SNSD record points to another hostname: ' + snsd_record.record)
            logging.debug('ANDNA: Its public key should be: ' + snsd_record.pubk.short_repr())
            response, resolved_record = self.resolve(snsd_record.record)
            if response == 'OK':
                if resolved_record.pubk != snsd_record.pubk:
                    # reject this request
                    # TODO
                    pass
            else:
                # the record has not been registered, so we cannot use it.
                # reject this request
                # TODO
                pass

        registered = False
        enqueued = False
        refused = False
        updated = False
        # check if already registered
        if self.cache.has_key(hostname):
            logging.debug('ANDNA: record already in cache...')
            # check if the registrar is the same
            old_record = self.cache[hostname]
            old_registrar = old_record.pubk
            # TODO verify that the == operator works with class PublicKey
            if old_registrar == pubk:
                # This is an update.
                logging.debug('ANDNA: ... this is an update')
                # Check and update the record in Counter node
                counter_gnode = self.counter.peer(key=sender_nip)
                logging.debug('ANDNA: contacting counter gnode')
                res, updates = counter_gnode.check(sender_nip, pubk, hostname, serv_key, IDNum,
                            snsd_record, signature)
                if not res:
                    raise AndnaError('ANDNA: Failed counter check.')
                # update record
                old_record.store(updates=IDNum, serv_key=serv_key, record=snsd_record)
                ret = 'OK', (registration_time, updates)
                updated = True
            else:
                logging.debug('ANDNA: ... and it is not from this pubk...')
                # check if we want and can append
                if append_if_unavailable:
                    if not self.request_queue.has_key(hostname):
                        self.request_queue[hostname] = []
                    if len(self.request_queue[hostname]) < self.max_andna_queue:
                        # append record
                        args_tuple_passed_to_register = (sender_nip,
                                hostname, service, pubk, signature,
                                snsd_record, snsd_record_pubk)
                        self.request_queue[hostname].append(args_tuple_passed_to_register)
                        logging.debug('ANDNA: ... request enqueued')
                        ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, request enqueued.'
                        enqueued = True
                    else:
                        logging.debug('ANDNA: ... queue is full')
                        ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue is full.'
                        refused = True
                else:
                    logging.debug('ANDNA: ... queue not requested')
                    ret = 'ALREADYREGISTERED', 'Hostname yet registered by someone, queue not requested.'
                    refused = True

        if not (enqueued or refused or updated):
            # Contact Counter node
            counter_node = self.counter.peer(key=sender_nip)
            logging.debug('ANDNA: contacting counter gnode')
            res, updates = counter_node.check(sender_nip, pubk, hostname, serv_key, IDNum,
                        snsd_record, signature)
            if not res:
                raise AndnaError('ANDNA: Failed counter check.')

            # register record
            logging.debug('ANDNA: registering ' + str(hostname))
            # This is the first registration of an hostname or an SNSD Node
            services = {}
            # TODO has the first registration to be necessarily a NULL_SERV_KEY?
            if serv_key == NULL_SERV_KEY and \
                    isinstance(snsd_record.record, str):
                logging.debug('ANDNA: The Zero Service record must be a NIP')
                # reject this request
                ret = 'NOTREGISTERED', 'The Zero Service record must be a NIP.'
            else:
                services[serv_key] = [snsd_record]
                self.cache[hostname] = \
                    AndnaAuthRecord(hostname, pubk, 0, 1, services)
                # AndnaAuthRecord(hostname, pubk, ttl, updates, services)
                self.resolved[(hostname, serv_key)] = \
                    self.cache[hostname].get_resolved_record(serv_key)
                registered = True

        if updated or registered:
            # forward the entry to my gnodes
            #self.forward_registration(hostname, self.cache[hostname])
            pass

        logging.debug('ANDNA: after reply_register: self.my_keys=' + str(self.my_keys))
        logging.debug('ANDNA: after reply_register: self.request_queue=' + str(self.request_queue))
        logging.debug('ANDNA: after reply_register: self.cache=' + str(self.cache))
        logging.debug('ANDNA: after reply_register: self.local_cache=' + str(self.local_cache))
        logging.debug('ANDNA: after reply_register: self.resolved=' + str(self.resolved))
        return ('OK', (registration_time, updates))

    def forward_registration(self, hostname, andna_cache):
        """ Broadcast the request to the entire gnode of level 1 to let the
           other nodes register the hostname. """
        me = self.mapp2p.me[:]
        for lvl in reversed(xrange(self.maproute.levels)):
            for id in xrange(self.maproute.gsize):
                nip = self.maproute.lvlid_to_nip(lvl, id)
                if self.maproute.nip_cmp(nip, me) == 0 and \
                    self.mapp2p.node_get(lvl, id).participant:    
                        remote = self.peer(hIP=nip)   
                        logging.debug("ANDNA: forwarding registration to `"+ \
                                  ip_to_str(self.maproute.nip_to_ip(nip))+"'")
                        res, data = remote.reply_forward_registration(
                                                        hostname, andna_cache)
                        
    def reply_forward_registration(self, hostname, andna_cache):
        # just add the entry into our database
        self.cache[hostname] = self.resolved[hostname] = andna_cache
        logging.debug("ANDNA: received a registration forward for `"+
                      hostname+"'")
        return ('OK', ())
                        
    def reply_queued_registration(self, hostname, timestamp, updates):
        """ The registration request we have sent and enqueued 
        when the hostname was busy has been satisfied now """
        # TODO: should I print a message to the user?
        self.local_cache[hostname] = timestamp, updates
        return ('OK', (timestamp, updates))    
    
    def resolve(self, hostname, service=0):
        """ Resolve the hostname, returns an SnsdRecord instance """
        logging.debug('ANDNA: resolve' + str((hostname, service)))
        res, data = '', ''
        # first try to resolve locally
        if self.resolved.has_key(hostname):
            return ('OK', self.resolved[hostname].services.get(service))
        # else call the remote hash gnode
        # calculate hash
        hash_node = self.peer(key=hostname)
        hash_nip = hash_node.get_hash_nip()
        logging.debug('ANDNA: exact hash_node is ' + str(hash_nip))
        # a random node of hash gnode of level 1.
        random_hnode = hash_nip[:]
        random_hnode[0] = randint(0, self.maproute.gsize)
        logging.debug('ANDNA: random hash_node is ' + str(random_hnode))
        # contact the hash gnode
        try:
            hash_gnode = self.peer(hIP=random_hnode)
            res, data = hash_gnode.reply_resolve(hostname, service)
        except Exception, e:
            logging.debug('ANDNA: could not resolve right now:')
            log_exception_stacktrace(e)
            res, data = 'CANTRESOLVE', 'Could not resolve right now'
        return (res, data)
        
    def reply_resolve(self, hostname, service):
        """ Return the list of SnsdRecord associated to the hostname and service """
        ret = ('NOTFOUND', 'Hostname not resolved.')
        andna_cache = None
        if hostname in self.cache:
            andna_cache = self.cache[hostname]
        # search into our resolved cache if nothing has been found
        elif hostname in self.resolved:
            andna_cache = self.resolved[hostname]
        if andna_cache:
            ret = ('OK', andna_cache.services.get(service))
        return ret
        
    def reverse_resolve(self, nip):
        """ Return the local cache of the specified host """
        remote = self.peer(hIP=self.maproute.ip_to_nip(str_to_ip(nip)))
        res, data = remote.reply_reverse_resolve()
        if res == 'OK':
            local_cache = data
            return (res, local_cache)
        return (res, data)
        
    def reply_reverse_resolve(self):
        return ('OK', self.local_cache)
    
    def get_resolved(self, nip):
        """ Return the resolved cache of the specified host """
        remote = self.peer(hIP=nip)
        res, data = remote.reply_resolved_cache()
        if res == 'OK':
            resolved_cache = data
            return (res, resolved_cache)
        return (res, data)
    
    def reply_resolved_cache(self):
        return ('OK', self.resolved_cache)
        
    def caches_merge(self, caches):
        resolved = cache = request_queue = {}
        if caches:
            resolved, cache, request_queue = caches
        if resolved:
            self.resolved.update(resolved)
            logging.debug("ANDNA: taken resolved cache from neighbour: "+
                          str(resolved))
        if cache:
            self.cache.update(cache)
            logging.debug("ANDNA: taken ANDNA cache from neighbour: "+
                          str(cache))
        if request_queue:
            self.request_queue.update(request_queue)
            logging.debug("ANDNA: taken request queue from neighbour: "+
                          str(request_queue))

    def cache_getall(self):
        return (self.resolved, self.cache, self.request_queue,)
    
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
