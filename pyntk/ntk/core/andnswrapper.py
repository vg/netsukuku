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

##
# This is a wrapper module for the extension (written by Efphe)
# that implements the ANDNS protocol.
#
# Here can be redirected and processed the ANDNS packets (in binary
# format) taken by a simple server socket.
##

import os
import sys
import andns.andns as andns 

from dns import message, rrset, query

from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc, micro_block
from ntk.lib.misc import is_ntk_name, parse_snsd_node, is_inaddr_arpa
from ntk.lib.rencode import dumps, loads
from ntk.lib.xtime import timestamp_to_data, today, days, swait
from ntk.network.inet import ip_to_str

# TODO: the public key of my neighbours must be saved on file! 
#       and in /etc/netsukuku/snsd_nodes them paths

class AndnsError(Exception): pass

class AndnsWrapper(object):
    
    def __init__(self, ntkd, andna, local_cache_path, nameservers=[], snsd_nodes=[]):
        self.ntkd = ntkd
        self.nameservers = nameservers
        self.andna = andna
        self.local_cache_path = local_cache_path
        if os.path.exists(local_cache_path):
            self.andna.local_cache = loads(open(local_cache_path, "rb").read())
        self.snsd_nodes = snsd_nodes
        self.stopped = True
        # remove from local cache the Snsd Nodes that are not in our conf file
        for hostname, andna_cache in self.andna.local_cache.items():
            if not hostname in [nodes[1] for nodes in self.snsd_nodes]:
                self.andna.local_cache.pop(hostname)
        self.andna.events.listen('ANDNA_HOOKED', self.andns_hook)
        # TODO: probably you need to listen ME_CHANGED after the ANDNA hook
        #self.andna.maproute.events.listen('ME_CHANGED', self.me_changed)

    @microfunc(True)
    def andns_hook(self):
        self.update_hostnames()
    
    @microfunc(True)
    def me_changed(self, old_me, new_me):
        self.update_hostnames()
     
    def update_hostnames(self):
        # wait at least one participant!
        while not self.andna.mapp2p.participant_list():
            swait(500)      
        # register new records retrieved from file
        for record in self.snsd_nodes:
            if record[2] == 'me':
                record[2] = ip_to_str(self.andna.maproute.nip_to_ip(
                                                   self.andna.maproute.me))
            append_if_unavailable, hostname, snsd_record, service, priority, \
                                 weight, snsd_record_pubk = record
            updates = 0
            if hostname in self.andna.local_cache:
                updates = self.andna.local_cache[hostname][1]
            res, data = self.register(hostname, service, updates, snsd_record, \
                            snsd_record_pubk, priority, weight, append_if_unavailable) 
            if res == 'OK':
                logging.debug("ANDNA: hostname "+str(record[1])+ \
                                    " registration number "+str(updates)+" completed")
            else:
                logging.debug("ANDNA: failed `"+str(record[1])+ \
                                  "' registration: "+str(data))
        if self.stopped:
            # TODO: self.stopped = False
            self.updater()
        
    def process_binary(self, request):
        """ Converts ANDNS packets (see RFC) to AndnsPacket objects """ 
        return self.process_packet(from_wired(request)).to_wire()

    def process_packet(self, packet):
        """ Process an AndnsPacket and returns it adding answers found. """
        query = { andns.NTK_REALM:  self.ntk_resolve,
                  andns.INET_REALM: self.inet_resolve }
        answers = []
        if packet.qr:
            # the packet has at least one question
            answers = query[packet.nk](packet.qstdata, packet.service, 
                          inverse=packet.qtype in [andns.AT_PTR, andns.AT_G])
        packet.rcode = 0
        for answer in answers:
            packet.addAnswer(answer)
        return packet
        
    def inet_resolve(self, name, service, inverse):
        logging.debug("ANDNS: Starting INET resolution of `"+str(name)+"'")
        answers = []
        for nameserver in self.nameservers:
            response = query.udp(response, nameserver)
            if request.is_answer(response):
                for rr in response.answer:
                    answers.append((1, 1, 1, service, str(rr.name))) 
            else:
                logging.debug("ANDNS: Failed DNS resolution using "+str(nameserver))
        return answers
            
    def ntk_resolve(self, name, service, inverse):
        """ Try to resolve the given name using ANDNA. """
        res = data = None
        answers = []
        logging.debug("ANDNS: Starting NTK resolution of `"+str(name)+"'")
        if inverse:
            res, data = self.andna.reverse_resolve(name)
        else:
            res, data = self.andna.resolve(name, service)
        if res != 'OK':
            logging.debug("ANDNS: NTK has not resolved `"+str(name)+"'")
        else:  
            answers.append((1, data.weight, data.priority, service, data.record))
        return answers

    def register(self, hostname, service, updates, snsd_record, 
                       snsd_record_pubk, priority, weight, append_if_unavailable):
        res, data = self.andna.register(hostname, service, updates, 
                                        snsd_record, snsd_record_pubk,
                                        priority, weight, append_if_unavailable)
        if res == 'OK':
            self.dump_caches()
        return res, data
        
    def dump_caches(self):
        open(self.local_cache_path, "wb").write(dumps(self.andna.local_cache))

    # TODO: complete
    @microfunc(True)
    def updater(self):
        """ Start a microthread that updates the last recently updated hostnames in my 
        own local cache. """
        while not self.stopped:
            logging.debug("ANDNA: hostname updater started")
            for hostname, (timestamp, updates) in self.andna.local_cache.items():
                if today() - timestamp_to_data(timestamp) > days(10):
                    res, data = self.register(hostname, updates,      ip_to_str(self.andna.maproute.nip_to_ip(self.andna.maproute.me)))
                    if res == 'OK':
                        logging.debug("ANDNA: "+str(updates)+" updated hostname `"+ \
                                       str(hostname)+"' from the updater daemon")
            # TODO: is it possible to suspend microfunc? manage with stackless.remove
            for i in range(900):
                micro_block()