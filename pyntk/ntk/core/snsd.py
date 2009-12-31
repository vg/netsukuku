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

# It is incomplete: probably the SNSD support must be removed 
# because SRV Records are no longer used by any application.

# TODO
#  you must verify that the service number is valid (manage with /etc/services)
#  use the limits (max_records, ecc) and raise exceptions

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.rencode import serializable

max_records = 256
max_records_for_service = 16
services_number_file_path = settings.SERVNO_PATH
services_num = []

class SnsdError(Exception): pass

def create_record(pubk, value, priority, weight):
    return SnsdRecord(pubk, value, priority, weight)
    
class SnsdRecord(object):
    
    def __init__(self, pubk, record, priority, weight):
        # RSA public key of the recorded node
        self.pubk = pubk
        # can be an ip or an hostname (always a string)
        self.record = record
        # A number that specifies the priority of the record in the list
        self.priority = priority
        # A number used when there are more than one records with the same priority
        self.weight = weight
    
    def __cmp__(self, b):
        """ The SnsdNode self is better (greater) than b if its priority is higher """
        if isinstance(b, SnsdRecord):
            res = cmp(self.priority, b.priority)
            if res == 0:
                return cmp(self.weight, b.weight)
            return res
        else:
            raise SnsdError('comparison with not SnsdRecord')
    
    def _pack(self):
        return (self.pubk, self.record, self.priority, self.weight,)
    
serializable.register(SnsdRecord)
        
class SnsdServices(object):
    
    def __init__(self, services={}):
        # key is the service number, and value a list of 
        # SnsdRecord ordered by priority and weight
        self.services = {}
        self.services.update(services)

    def store(self, servno, record):
        """ record is an istance of SnsdRecord """
        if not isinstance(servno, int):
            raise SnsdError('invalid service number specified')
        elif not servno in self.services:
            self.services[servno] = []
    
        # append the record to the array
        self.services[servno].append(record)
        # sort list by priority
        self.services[servno].sort(reverse=1)
  
    def get(self, servno):
        """ Return the SnsdRecord with the highest priority """
        return self.services[servno][0]

    def get_all(self):
        """ Return all the records of all the services """
        return [ (k, v) for k, v in self.services.items() ]
    
    def _pack(self):
        return (self.services,)

serializable.register(SnsdServices)