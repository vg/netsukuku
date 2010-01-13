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

import ntk.lib.rencode as rencode
from ntk.lib.rencode import serializable
from ntk.core.andna import hash_32bit_ip
from ntk.core.p2p import OptionalP2P
from ntk.lib.crypto import md5, verify
from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc
from ntk.wrap.xtime import (now, timestamp_to_data, today, days, 
                            while_condition, time)
from ntk.core.snsd import SnsdRecord, MAX_TTL_OF_NEGATIVE, \
            MAX_TTL_OF_REGISTERED, AndnaAuthRecord, AndnaResolvedRecord

MAX_HOSTNAMES = 256

class CounterError(Exception): pass

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

        # The counter cache = { (str(pubk), str(nip)): CounterAuthRecord, ... }
        self.cache = {} 

        self.remotable_funcs += [self.check, self.cache_getall,
                                self.reply_forward_registration]

    @microfunc(True) 
    def counter_hook(self):
        # clear old cache
        self.reset()
        logging.debug('COUNTER: resetted.')
        logging.debug('COUNTER: after reset: self.cache=' + str(self.cache))

        # # merge ??
        # neigh = None
        # def no_participants():
        #     return self.mapp2p.node_nb[self.mapp2p.levels-1] >= 1
        # # wait at least one participant
        # while_condition(no_participants)
        # for neigh in self.neigh.neigh_list(in_my_network=True):
        #     nip = self.maproute.ip_to_nip(neigh.ip)
        #     peer = self.peer(hIP=nip)
        #     self.caches_merge(peer.cache_getall())

    def reset(self):
        self.cache = {}

    def check_expirations(self):
        # Remove the expired entries from the COUNTER cache
        logging.debug('COUNTER: cleaning expired entries - if any - from our COUNTER cache...')
        for (str_pubk, str_nip), auth_record in self.cache.items():
            pubk, nip = auth_record.pubk, auth_record.nip
            if not auth_record.has_any():
                self.cache.pop((str_pubk, str_nip))
                logging.debug('COUNTER: cleaned entry (pubk ' + pubk.short_repr() + \
                        ', nip ' + str(self.nip) + ')')

    def check(self, sender_nip, pubk, hostname, serv_key, IDNum,
                       snsd_record, signature):
        """ Return a tuple like (True/False, updates) """
        # Remove the expired entries from the COUNTER cache
        self.check_expirations()
        # Check that the message comes from this pubk
        logging.debug('COUNTER: authenticating the request...')
        if not verify(rencode.dumps((sender_nip, hostname, serv_key, IDNum,
                       snsd_record)), signature, pubk):
            raise CounterError('Request authentication failed')
        logging.debug('COUNTER: request authenticated')
        # Check that the NIP is assigned to this pubk
        # TODO
        # Retrieve data, update data, prepare response
        strpubk = str(pubk)
        strnip = str(sender_nip)
        if (strpubk, strnip) not in self.cache:
            self.cache[(strpubk, strnip)] = CounterAuthRecord(pubk, sender_nip, {})
        ret = self.cache[(strpubk, strnip)].store(hostname)
        # Forward registration
        # self.forward_registration(public_key, 
        #                                  hostname, 
        #                                  self.cache[public_key][hostname])

        return (ret, IDNum)

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
    
    def h(self, nip):
        """ Retrieve an IP from the NIP """
        # TODO check!
        return hash_32bit_ip(md5(str(nip)), self.maproute.levels, 
                             self.maproute.gsize)
