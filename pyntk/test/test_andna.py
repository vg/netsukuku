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

import os
import sys
import unittest

sys.path.append('..')

from etp_simulator import initialize
from network_simulator import create_nodes, create_network, create_service

from ntk.core.andna import AndnaError
from ntk.lib.log import init_logger
from ntk.lib.micro import micro_block


init_logger()
initialize()

# Topology of the network
#    1---2
#     \ /
#      3---6
#     /   /
#    4---5

class TestAndna(unittest.TestCase):

    def setUp(self):
        self.keys_path = os.getcwd() + "/keys"        
        self.conf_path = os.getcwd() + "/conf"
        self.localcache_path = os.getcwd() + "/local"
        self.nics = ['This', 'are', 'just', 'few', 'fake', 'interfaces']
        self.devs = {}
        for nic in self.nics:
            self.devs[nic] = 10 # fake average value
        self.bestdev = ('fake', 10)
        self.total_nodes = 6
        ip_list = []
        for i in xrange(self.total_nodes):
            ip_list.append([i+11, 9, 8, 7])
        self.nodes = create_service(create_network(
                       create_nodes(self.keys_path, self.localcache_path, 
                       self.conf_path, self.total_nodes, ip_list)))
                       
        self.me     = self.nodes[1]
        self.remote = self.nodes[2]

    def tearDown(self):
        for idn, node in self.nodes.items():
            node_keys_path = self.keys_path + "/" + str(idn)
            files = os.listdir(node_keys_path)
            for f in files:
                os.remove(node_keys_path + "/" + f)
            os.rmdir(node_keys_path)
            node_conf_path = self.conf_path + "/" + str(idn)
            files = os.listdir(node_conf_path)
            for f in files:
                os.remove(node_conf_path + "/" + f)
            os.rmdir(node_conf_path)
            node_localcache_path = self.localcache_path + "/" + str(idn)
            files = os.listdir(node_localcache_path)
            for f in files:
                os.remove(node_localcache_path + "/" + f)
            os.rmdir(node_localcache_path)
        os.rmdir(self.keys_path)
        os.rmdir(self.conf_path)
        os.rmdir(self.localcache_path)

    def testRegistration(self, client=None, hostname="Testing", service=0, 
                         IDNum=0, snsd_record='', snsd_record_pubk=None, 
                         priority=1, weight=1, append_if_unavailable=False):    
        if client is None:
            client = self.me
        (res, data) = client.andna.register(hostname, service, IDNum, 
                                            snsd_record, snsd_record_pubk, 
                                            priority, weight,
                                            append_if_unavailable)
        if res == 'OK':
            timestamp, updates = data
        elif res == 'rmt_error':
            return res, AndnaError(data)
        return (res, (timestamp, updates))
    
    def testHostnameHash(self, client=None, register_node=None, 
                           hostname="Testing"):
        if client is None:
            client = self.me
        if register_node is None:
            register_node = self.remote
        hash_gnode1 = client.andna.H(client.andna.h(hostname)) 
        hash_gnode2 = register_node.andna.H(register_node.andna.h(hostname))
        self.failUnlessEqual(hash_gnode1 == hash_gnode2, True)
         
    def testResolution(self):
        snsd_node = None
        hostname = "hostname for self.me"
        self.testRegistration(self.me, hostname, snsd_record="7.8.9.12")
        res, data = self.remote.andna.resolve(hostname)
        if res == 'OK':
            snsd_node = data
        self.failUnlessEqual(snsd_node.record == "7.8.9.12", True)
        
    def testReverseResolution(self):
        local_cache_of_remote_host = None
        self.testRegistration()
        res, data = self.remote.andna.reverse_resolve("7.8.9.12")
        if res == 'OK':
            local_cache_of_remote_host = data
        # print the local cache of the remote host
        for hostname, (timestamp, updates) in \
            local_cache_of_remote_host.items():
            print ("Hostname: ", hostname, " Last update: ", timestamp, 
                    " Updates done: ", updates )
        
    def testSnsdNodes(self):
        keys = self.me.andna.my_keys
        # self.me contact the register node to register the SNSD Node
        self.testRegistration(self.me,
                              hostname="Test", 
                              service=80, 
                              snsd_record="webserver",  
                              snsd_record_pubk=keys.get_pub_key())
        # This line is in /etc/netsukuku/snsd_nodes of the register node:
        #    webserver:Test:80:priority:weight
        # Note: `webserver' is the SNSD Node.
        # self.remote contact the register node to resolve Test:80,
        # thus the SNSD Node `webserver' is returned. 
        res, data = self.remote.andna.resolve(hostname="Test", service=80)
        if res == 'OK':
            snsd_record = data
            self.failUnlessEqual(snsd_record.record == "webserver", True)
        # Now should continue to resolve `webserver'.
        
    def testHostnameLimit(self):
        limit = 5
        for idn, node in self.nodes.items():
            node.counter.max_hostnames_limit = limit
        for i in xrange(limit+5):
            res, data = self.testRegistration(self.me, hostname="hostname"+
                                              str(i), 
                                              snsd_record="123.123.123.123")
            self.failIfEqual(i > limit and res == 'OK', True)
            if res != 'OK':
                self.failIfEqual('Hostnames limit reached' in str(data), 
                                 False)
        
    def testMaxQueueHostname(self):
        hostname = "always the same"
        register_node_tmp = self.nodes[1].andna.H(
                              self.nodes[1].andna.h(hostname))
        for i in xrange(self.total_nodes):
            # the register node must be the same for all the client node
            register_node = self.nodes[i].andna.H(
                              self.nodes[i].andna.h(hostname))
            self.failUnlessEqual(register_node_tmp == register_node, True)
            res, data = self.testRegistration(self.nodes[i], hostname, 
                                              append_if_unavailable=True)
            if i > self.me.andna.max_andna_queue:
                self.failIfEqual(res != 'OK' and 'enqueued' in str(data), 
                                 False)
    
    def testHooking(self):
        # We don't use self.testRegistration, because the request is forwarded
        # into the gnode, we will directly write the entry into the cache 
        # of the neighbour 2, then the node 3 try to hook it and will 
        # take its cache
        self.nodes[2].andna.cache['taken by node 2'] = 'Fake AndnaCache value'
        # node 3 see node 2
        self.nodes[3].andna.neigh.send_event_neigh_new(self.bestdev, 
                    self.devs, 2, 
                    self.nodes[3].maproute.nip_to_ip(self.nodes[2].firstnip), 
                    netid=123)
        # let's hook
        self.nodes[3].andna.p2pall.events.send('P2P_HOOKED', ())
        micro_block()
        res = self.nodes[3].andna.cache.has_key('taken by node 2')
        self.failIfEqual(res, False)
                
    def testForwarding(self):
        hostname = "host"
        self.testRegistration(hostname=hostname)
        res = True
        for idn in xrange(self.total_nodes-1):
            if self.nodes[idn].andna.cache.has_key(hostname) and \
               self.nodes[idn+1].andna.cache.has_key(hostname):
                res = self.nodes[idn].andna.cache[hostname] == \
                      self.nodes[idn+1].andna.cache[hostname]
                self.failIfEqual(res, False)
    
if __name__ == '__main__':
    unittest.main()