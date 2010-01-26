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

# TODO
#  you must verify that the service number is valid (manage with /etc/services)
#  use the limits (max_records, ecc) and raise exceptions

# The TTL of a record that has just been registered.
MAX_TTL_OF_REGISTERED = 1000 * 60 * 60 * 24 * 30 # 30 days in millisec
# The maximum TTL of a cached record.
MAX_TTL_OF_RESOLVED = 1000 * 60 * 5 # 5 minutes in millisec
# The maximum TTL of a negative caching (hostname not registered).
MAX_TTL_OF_NEGATIVE = 1000 * 30 # 30 seconds in millisec

from ntk.lib.rencode import serializable
from ntk.wrap.xtime import time
from ntk.config import settings

class AndnaError(Exception): pass

class SnsdAuthRecord:
    def __init__(self, record, pubk, priority, weight):
        # string record = it can be None or an further hostname
        #   If record is None then this record refers to the NIP of
        #   the registrar. Otherwise it is an hostname, then this
        #   record refers to that hostname.
        self.record = record
        # PublicKey pubk = public key of record.
        #   If record is an further hostname, then pubk is the public
        #   key of the host registrar of that hostname. The value
        #   of pubk in this case is specified by the client and verified
        #   by the hash node.
        #   If record is None, then pubk is None.
        self.pubk = pubk
        # int priority = the priority of the record (lower is better)
        self.priority = priority
        # int weight = the relative weight of the record, between records with
        #   the same priority.
        self.weight = weight
        # in future we should add the field port_number

    def __repr__(self):
        pubk_str = 'None'
        if self.pubk is not None:
            pubk_str = self.pubk.short_repr()
        ret = '<SnsdAuthRecord: (record ' + str(self.record) + \
                ', pubk ' + pubk_str + \
                ', priority ' + str(self.priority) + \
                ', weight ' + str(self.weight) + ')>'
        return ret

    def _pack(self):
        return (self.record, self.pubk, self.priority, self.weight)

serializable.register(SnsdAuthRecord)

class AndnaAuthRecord:
    def __init__(self, hostname, pubk, nip, ttl, updates, services):
        # string hostname = hostname
        self.hostname = hostname
        # int ttl = millisec to expiration
        #           if 0 => MAX_TTL_OF_REGISTERED
        if ttl == 0 or ttl > MAX_TTL_OF_REGISTERED:
            ttl = MAX_TTL_OF_REGISTERED
        self.expires = time() + ttl
        # int updates = number of updates
        self.updates = updates
        # PublicKey pubk = public key of registrar
        self.pubk = pubk
        # NIP nip = current nip of registrar
        self.nip = nip
        # dict<serv_key,sequence<SnsdAuthRecord>> services = la chiave è il service_number
        #               in futuro potrebbe essere la tupla (service_name, protocol_name)
        self.services = {}
        self.services.update(services)
    
    def __repr__(self):
        ret = '<AndnaAuthRecord: (hostname ' + str(self.hostname) + \
                ', pubk ...' + self.pubk.short_repr() + \
                ', nip ' + str(self.nip) + \
                ', ttl ' + str(self.get_ttl()) + \
                ', updates ' + str(self.updates) + \
                ', services ' + str(self.services) + ')>'
        return ret

    def store(self, updates, serv_key, record):
        """ Updates a registration """
        # TODO: check validity of serv_key

        if record.record is None:
            record.pubk = None
        if record.pubk is None and record.record is not None:
            raise AndnaError, 'Public key of pointed hostname was not specified.'

        # updates registration
        # TODO: check updates is 1+self.updates ?
        self.updates = updates
        self.expires = time() + MAX_TTL_OF_REGISTERED

        if serv_key not in self.services:
            self.services[serv_key] = []
        else:
            # TODO find the record if it is already there
            pass

        # append the record to the array
        self.services[serv_key].append(record)

    # TODO def remove(self, updates, serv_key, record):

    def get_all(self):
        """ Return all the records of all the services """
        # TODO: remove the method if we don't use.
        return [ (k, v) for k, v in self.services.items() ]

    def get_resolved_record(self, serv_key):
        if serv_key in self.services:
            return AndnaResolvedRecord(self.get_ttl(), self.services[serv_key])
        else:
            return AndnaResolvedRecord(self.get_ttl(), None)

    def get_ttl(self):
        return self.expires - time()

    def _pack(self):
        services = {}
        services.update(self.services)
        return (self.hostname, self.pubk, self.nip[:], self.get_ttl(), self.updates, services)

serializable.register(AndnaAuthRecord)

class SnsdResolvedRecord:
    def __init__(self, record, pubk, priority, weight):
        # string/NIP record = it can be a NIP or an further hostname
        self.record = record[:]
        # int priority = the priority of the record (lower is better)
        self.priority = priority
        # int weight = the relative weight of the record, between records with
        # the same priority.
        self.weight = weight
        # in future we should add the field port_number

    def __repr__(self):
        pubk_str = 'None'
        if self.pubk is not None:
            pubk_str = self.pubk.short_repr()
        ret = '<SnsdResolvedRecord: (record ' + str(self.record) + \
                ', priority ' + str(self.priority) + \
                ', weight ' + str(self.weight) + ')>'
        return ret

    def _pack(self):
        return (self.record[:], self.priority, self.weight)

serializable.register(SnsdResolvedRecord)

class AndnaResolvedRecord:
    # This class represent the resolution of an hostname and a serv_key.
    # It is normally returned by the hash node as the result of a request
    # for hostname+serv_key. But the same class can be used by a node to
    # cache the resolved queries. In this case an instance for each pair
    # (hostname, serv_key) has to be maintained.
    # This class does not contain the hostname and the serv_key themselves.
    def __init__(self, ttl, services):
        # int ttl = millisec to expiration
        self.expires = time() + min(ttl, MAX_TTL_OF_RESOLVED)
        # sequence<SnsdResolvedRecord> services = i record per questo (hostname, serv_key)
        #                                 oppure None se non sono stati registrati
        #      it can be None.
        services = services[:] if services is not None else None
        self.services = services
    
    def __repr__(self):
        ret = '<AndnaResolvedRecord: (ttl ' + str(self.expires - time()) + \
                ', services ' + str(self.services)+ ')>'
        return ret

    def _pack(self):
        services = self.services[:] if self.services is not None else None
        return (self.expires - time(), services)

serializable.register(AndnaResolvedRecord)

