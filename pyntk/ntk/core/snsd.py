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

services_number_file_path = settings.SERVNO_PATH


class SnsdRecord:
    def __init__(self, record, pubk, priority, weight):
        # string/NIP record = può essere un NIP o un ulteriore hostname solo se serv_key
        #               è diverso da 0 (oppure in futuro la tupla è diversa da None)
        #               altrimenti è un NIP
        self.record = record[:]
        # PublicKey pubk = public key of record, se record è un ulteriore hostname,
        #               altrimenti è None
        self.pubk = pubk
        # int priority = priorita', dal basso verso l'alto
        self.priority = priority
        # int weight = peso relativo entro la stessa priorita'
        self.weight = weight
        # in futuro si potrebbe aggiungere il campo port_number

        # TODO: if record is a NIP
        #       then check that pubk is None, and vice-versa.

    def __repr__(self):
        pubk_str = 'None'
        if self.pubk is not None:
            pubk_str = self.pubk.short_repr()
        ret = '<SnsdRecord: (record ' + str(self.record) + \
                ', pubk ' + pubk_str + \
                ', priority ' + str(self.priority) + \
                ', weight ' + str(self.weight) + ')>'
        return ret

    def _pack(self):
        return (self.record[:], self.pubk, self.priority, self.weight)

serializable.register(SnsdRecord)

class AndnaAuthRecord:
    def __init__(self, hostname, pubk, ttl, updates, services):
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
        # dict<serv_key,sequence<SnsdRecord>> services = la chiave è il service_number
        #               in futuro potrebbe essere la tupla (service_name, protocol_name)
        self.services = {}
        self.services.update(services)
    
    def __repr__(self):
        ret = '<AndnaAuthRecord: (hostname ' + str(self.hostname) + \
                ', pubk ...' + self.pubk.short_repr() + \
                ', ttl ' + str(self.get_ttl()) + \
                ', updates ' + str(self.updates) + \
                ', services ' + str(self.services)+ ')>'
        return ret

    def store(self, updates, serv_key, record):
        """ Updates a registration """
        # TODO: check validity of serv_key

        # TODO: record is an instance of SnsnRecord. If serv_key is 0
        #       (or None, see above) then check that record.record is a NIP.

        # TODO: record is an instance of SnsnRecord. If record.record is a NIP
        #       then check that record.pubk is None, and vice-versa.

        # updates registration
        # TODO: check updates is 1+self.updates ?
        self.updates = updates
        self.expires = time() + MAX_TTL_OF_REGISTERED

        if serv_key not in self.services:
            self.services[serv_key] = []

        # append the record to the array
        self.services[serv_key].append(record)

    def get_all(self):
        """ Return all the records of all the services """
        # TODO: remove the method if we don't use.
        return [ (k, v) for k, v in self.services.items() ]

    def get_resolved_record(self, serv_key):
        if serv_key in self.services:
            return AndnaResolvedRecord(self.hostname, serv_key, self.pubk, self.get_ttl(), self.services[serv_key])
        else:
            return AndnaResolvedRecord(self.hostname, serv_key, self.pubk, self.get_ttl(), None)

    def get_ttl(self):
        return self.expires - time()

    def _pack(self):
        services = {}
        services.update(self.services)
        return (self.hostname, self.pubk, self.get_ttl(), self.updates, services)

serializable.register(AndnaAuthRecord)

class AndnaResolvedRecord:
    def __init__(self, hostname, serv_key, pubk, ttl, services):
        # string hostname = hostname
        self.hostname = hostname
        # serv_key = la chiave è il service_number
        #               in futuro potrebbe essere la tupla (service_name, protocol_name)
        self.serv_key = serv_key
        # int ttl = millisec to expiration
        self.expires = time() + min(ttl, MAX_TTL_OF_RESOLVED)
        # PublicKey pubk = public key of registrar
        self.pubk = pubk
        # sequence<SnsdRecord> services = i record per questo (hostname, serv_key)
        #                                 oppure None se non sono stati registrati
        self.services = services[:]
    
    def __repr__(self):
        ret = '<AndnaResolvedRecord: (hostname ' + str(self.hostname) + \
                ', serv_key ' + str(self.serv_key) + \
                ', pubk ...' + self.pubk.short_repr() + \
                ', ttl ' + str(self.expires - time()) + \
                ', services ' + str(self.services)+ ')>'
        return ret

    def _pack(self):
        return (self.hostname, self.serv_key, self.pubk, self.expires - time(), self.services[:])

serializable.register(AndnaResolvedRecord)

