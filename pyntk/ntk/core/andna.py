##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
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

from ntk.core.p2p import P2P
from ntk.core.snsd import SnsdServices, create_record
from ntk.lib.crypto import md5, fnv_32_buf, verify
from ntk.lib.event import Event
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc
from ntk.lib.misc import is_ip
from ntk.lib.rencode import serializable
from ntk.network.inet import ip_to_str, str_to_ip
from ntk.wrap.xtime import timestamp_to_data, today, days, while_condition

# TODO:
# - keep the public key of my neighbours on file, adding them path 
#   to /etc/netsukuku/snsd_nodes
# - add support for IPv6 (hash function, ecc)

class AndnaError(Exception): pass

class AndnaCache(object):
    
    def __init__(self, timestamp, updates, services, pubk):
        self.timestamp = timestamp
        self.updates   = updates
        self.services  = services
        self.pubk      = pubk
    
    def _pack(self):
        return (self.timestamp, self.updates, self.services, self.pubk,)

serializable.register(AndnaCache)
    
class Andna(P2P):
    
    pid = 3
    
    def __init__(self, ntkd_status, keypair, counter, radar, maproute, p2pall):
        P2P.__init__(self, ntkd_status, radar, maproute, Andna.pid)
        
        self.p2pall = p2pall
        
        # let's register ourself in p2pall
        self.p2pall.p2p_register(self)
        self.p2pall.events.listen('P2P_HOOKED', self.andna_hook)
        
        self.events = Event(['ANDNA_HOOKED'])
        
        self.expiration_days = counter.expiration_days       
        self.max_andna_queue = 5
        
        self.counter = counter
        self.my_keys = keypair
        # Appended requests = { hostname: 
        #                       args_tuple_passed_to_register, ... }
        self.request_queue = {}
        # Andna Cache = { hostname: AndnaCache(), ... }
        self.cache = {} 
        # Local Cache = { hostname: (timestamp, updates), ... }
        self.local_cache = {}
        # Resolved Cache = { hostname: AndnaCache(), ... }
        self.resolved = {}
    
        self.remotable_funcs += [self.reply_register, self.reply_resolve,
                                 self.reply_reverse_resolve, 
                                 self.reply_queued_registration,
                                 self.reply_resolved_cache,
                                 self.reply_forward_registration,
                                 self.cache_getall]
        
    @microfunc(True)    
    def andna_hook(self):
        neigh = None
        def no_participants():
            return self.mapp2p.node_nb[self.mapp2p.levels-1] >= 1
        # wait at least one participant
        while_condition(no_participants)
        for neigh in self.neigh.neigh_list(in_my_network=True):
            nip = self.maproute.ip_to_nip(neigh.ip)
            peer = self.peer(hIP=nip)
            self.caches_merge(peer.cache_getall())
        self.events.send('ANDNA_HOOKED', ())
        
    def participate(self):
        """Let's become a participant node"""
        P2P.participate(self)  # base method
        
    def register(self, hostname, service, IDNum, snsd_record,
        snsd_record_pubk, priority, weight, append_if_unavailable):
        """ Register or update the name for the specified service number """
        # contact the hash gnode firstly
        hash_gnode = self.peer(key=hostname)
        if hash_gnode.peer_is_me(): 
            logging.debug("ANDNA: Failed. Hash gnode is me!")
            return ("KO", "There are no participants")
        # sign the request and attach the public key
        signature = self.my_keys.sign(hostname)
        res, msg = hash_gnode.reply_register(self.maproute.me[:],
                                              hostname, 
                                              service, 
                                              self.my_keys.get_pub_key(), 
                                              signature,
                                              IDNum,
                                              snsd_record,
                                              snsd_record_pubk,
                                              priority,
                                              weight,
                                              append_if_unavailable)
        if res == 'rmt_error':
            return (res, msg)
        timestamp, updates = self.local_cache[hostname] = msg
        return (res, (timestamp, updates))
            
    def reply_register(self, sender_nip, hostname, service, pubk, signature, 
        IDNum, snsd_record, snsd_record_pubk, priority, weight, 
        append_if_unavailable):
        # Remove the expired entries from the ANDNA cache 
        for hname, andna_cache in self.cache.items():
            if (today() - timestamp_to_data(andna_cache.timestamp) > 
                                           days(self.expiration_days)):
                    self.cache.pop(hname)
                    if self.request_queue.has_key(hname):
                        res, data = self.reply_register(
                                        self.request_queue[hname], 
                                        append_if_unavailable=False)
                    if res == 'OK':
                        sender_nip = self.request_queue[hname][0]
                        client = self.peer(hIP=sender_nip)
                        timestamp, updates = data
                        client.reply_queued_registration(hname, timestamp, 
                                                         updates)
        logging.debug("ANDNA: cleaned expired entries from our ANDNA cache")                        
        registration_time = updates = 0
        # first the authentication check
        if not verify(hostname, signature, pubk):
            raise AndnaError("Request authentication failed")
        if self.cache.has_key(hostname) and append_if_unavailable:
            if not self.request_queue.has_key(hostname):
                self.request_queue[hostname] = []
            if len(self.request_queue[hostname]) < self.max_andna_queue:
                self.request_queue[hostname].append((sender_nip, 
                                                     hostname, 
                                                     service,
                                                     pubk, 
                                                     signature, 
                                                     snsd_record, 
                                                     snsd_record_pubk),)
                raise AndnaError("Hostname yet registered by someone, "
                                 "request enqueued.")
        # contact the counter gnode
        counter_gnode = self.counter.peer(key=pubk)
        logging.debug("ANDNA: contacting counter gnode")
        res, data = counter_gnode.check(pubk, hostname, signature, IDNum) 
        if res == 'OK':
            logging.debug("ANDNA: counter gnode check it's ok")
            registration_time, updates = data
        elif res == 'rmt_error':            
            logging.debug("ANDNA: counter gnode check failed")
            raise AndnaError("Failed counter check: "+str(data))
        if updates == 1:
            logging.debug("ANDNA: this is the first registration for the "
                          "name "+str(hostname))
            # This is the first registration of an hostname or an SNSD Node
            services = SnsdServices()
            if snsd_record is None:
                logging.debug("ANDNA: replacing SNSD record with sender nip")
                snsd_record = ip_to_str(self.maproute.nip_to_ip(sender_nip))
            if service == 0 and not is_ip(snsd_record):
                raise AndnaError("The Zero Service record must be an IP")
            services.store(service, create_record(pubk, snsd_record, priority,
                                                   weight))
            self.cache[hostname] = self.resolved[hostname] = \
                AndnaCache(registration_time, updates, services, pubk)                  
        elif updates > 1:
            logging.debug("ANDNA: we are updating the name "+str(hostname))
            # We are updating an entry now
            self.cache[hostname].timestamp = registration_time
            self.cache[hostname].updates = updates
            services = self.cache[hostname].services
            if snsd_record is not None and snsd_record_pubk is not None:
                services.store(service, create_record(snsd_record_pubk, 
                                                      snsd_record, 
                                                      priority, weight))

        # forward the entry to my gnodes
        self.forward_registration(hostname, self.cache[hostname])
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
        # first try to resolve locally
        if self.resolved.has_key(hostname):
            return ('OK', self.resolved[hostname].services.get(service))
        # else call the remote hash gnode
        hash_gnode = self.peer(key=hostname)
        if hash_gnode.peer_is_me(): 
            return ("KO", AndnaError("There are no participants?"))
        res, data = hash_gnode.reply_resolve(hostname, service)
        if res == 'OK':
            snsd_record = data
            return (res, snsd_record)
        return (res, data)
        
    def reply_resolve(self, hostname, service):
        """ Return the SnsdRecord associated to the hostname """
        for hostname, andna_cache in self.cache.items():
            return ('OK', andna_cache.services.get(service))
        # search into our resolved cache if nothing has been found
        if self.resolved.has_key(hostname):
            return ('OK', self.resolved[hostname].services.get(service))
        raise AndnaError("Not resolved.")
        
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
