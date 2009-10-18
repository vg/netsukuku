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

from ntk.core.andna import hash_32bit_ip
from ntk.core.p2p import P2P
from ntk.lib.crypto import md5, verify
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc
from ntk.wrap.xtime import (now, timestamp_to_data, today, days, 
                            while_condition)

class CounterError(Exception): pass

class Counter(P2P):
    
    pid = 2
    
    def __init__(self, keypair, radar, maproute, p2pall):
        P2P.__init__(self, radar, maproute, Counter.pid)
        # period starting from registration time within which
        # the hostname must be updated
        self.expiration_days = 30                 
        self.max_hostnames_limit = 256
        self.my_keys = keypair
        self.p2pall = p2pall
        
        # let's register ourself in p2pall
        self.p2pall.p2p_register(self)
        self.p2pall.events.listen('P2P_HOOKED', self.counter_hook)
        
        # The counter cache = { RSA public key: 
        #                       { hostname: (timestamp, updates), ... }, 
        #                       ... 
        #                     }
        self.cache = {} 
        
        self.remotable_funcs += [self.check, self.cache_getall,
                                self.reply_forward_registration]
    
    @microfunc(True) 
    def counter_hook(self):
        neigh = None
        def no_participants():
            return self.mapp2p.node_nb[self.mapp2p.levels-1] >= 1
        # wait at least one participant
        while_condition(no_participants)
        for neigh in self.neigh.neigh_list(in_my_network=True):
            nip = self.maproute.ip_to_nip(neigh.ip)
            peer = self.peer(hIP=nip)
            self.caches_merge(peer.cache_getall())
            
    def participate(self):
        """Let's become a participant node"""
        P2P.participate(self)  # base method
        
    def check(self, public_key, hostname, signature, IDNum):
        """ Return a tuple like (res, (timestamp, updates)) """
        # Remove the expired entries from the counter cache 
        for pubk, hostnames in self.cache.items():
            for hname, (timestamp, updates) in hostnames.items():
                if (today() - timestamp_to_data(timestamp) > 
                                     days(self.expiration_days)):
                    self.cache[pubk].pop(hname)
        
        if not verify(hostname, signature, public_key):
            raise CounterError("Request authentication failed")
        if self.cache.has_key(public_key) and \
           len(self.cache[public_key]) > self.max_hostnames_limit:
            raise CounterError("Hostnames limit reached")        
        if not self.cache.has_key(public_key):
            self.cache[public_key] = {}
        if self.cache.has_key(public_key) and \
           self.cache[public_key].has_key(hostname):
            # This is a check from an update request of an existent hostname
            timestamp, updates = self.cache[public_key][hostname]
            if updates == IDNum:
                self.cache[public_key][hostname] = (now(), updates+1)
                self.forward_registration(public_key, 
                                          hostname, 
                                          self.cache[public_key][hostname])
                return ('OK', self.cache[public_key][hostname])
            raise CounterError("The id is not correct. Failed.")
        elif IDNum == 0:
            # This is the first check of the hostname
            self.cache[public_key].update({hostname: (now(), 1)})
            self.forward_registration(public_key, 
                                      hostname, 
                                      self.cache[public_key][hostname])
            return ('OK', self.cache[public_key][hostname])
                        
        # reject request
        raise CounterError("Expired hostname. You should register again.")
    
    def forward_registration(self, public_key, hostname, entry):
        """ Broadcast the request to the entire gnode of level 1 to let the
           other counter_nodes register the hostname. """
        me = self.mapp2p.me[:]
        for lvl in reversed(xrange(self.maproute.levels)):
            for id in xrange(self.maproute.gsize):
                nip = self.maproute.lvlid_to_nip(lvl, id)
                if self.maproute.nip_cmp(nip, me) == 0 and \
                    self.mapp2p.node_get(lvl, id).participant:    
                        remote = self.peer(hIP=nip)
                        res, data = remote.reply_forward_registration(
                                            public_key, hostname, entry)
                        
    def reply_forward_registration(self, public_key, hostname, entry):
        # just add the entry into our database
        if not self.cache.has_key(public_key):
            self.cache[public_key] = {}
        self.cache[public_key][hostname] = entry
        return ('OK', ())
    
    def caches_merge(self, caches):
        cache = caches
        if cache:
            self.cache.update(cache)
            logging.debug("Counter: taken cache from neighbour: "+str(cache))
    
    def cache_getall(self):
        return (self.cache)
    
    def h(self, pubk):
        """ Retrieve an IP from the RSA public key """
        return hash_32bit_ip(md5(str(pubk)), self.maproute.levels, 
                             self.maproute.gsize)