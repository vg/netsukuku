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

from random import choice
import os
import sys
import unittest

from etp_simulator import create_node, initialize
from ntk.config import settings
from ntk.core.andna import Andna, AndnaError
from ntk.core.counter import Counter
from ntk.core.p2p import P2PAll
from ntk.core.qspn import Etp
from ntk.core.route import MapRoute, Rtt
from ntk.core.radar import Neighbour, Neigh
from ntk.lib.log import init_logger
from ntk.wrap.xtime import swait

sys.path.append('..')
init_logger()
initialize()

# Topology of the network
#    1---2
#     \ /
#      3---6
#     /   /
#    4---5
# reused lukisi's code from etp_simulator.py 
# (TODO: create_node function has been modified a bit)

class TestAndna(unittest.TestCase):
    
    def setUp(self):
        self.keys_path = os.getcwd() + "/keys"
        self.nics = ['This', 'are', 'just', 'few', 'fake', 'interfaces']
        self.devs = {}
        for nic in self.nics:
            self.devs[nic] = 10 # fake average value
        self.bestdev = ('fake', 10)
        self.nodes = {} # item = idn: FakeNtkNode
        for idn in range(1, 7):
            node_keys_path = self.keys_path + "/" + str([idn+11, 9, 8, 7])
            if not os.path.exists(self.keys_path):
                os.mkdir(self.keys_path)
            if not os.path.exists(node_keys_path):
                os.mkdir(node_keys_path)
            else:
                files = os.listdir(node_keys_path)
                for f in files:
                    os.remove(node_keys_path + "/" + f)
            self.nodes[idn] = create_node([idn+11, 9, 8, 7], 
                                          self.nics[idn-1],
                                          netid=123, 
                                          id=idn,
                                          keypair_path=node_keys_path+"/keypair")
        self.me     = self.nodes[1]
        self.remote = self.nodes[2]

        def add_routes(node, nip, gw):
            for lvl in xrange(node.maproute.levels):
                neigh = Neigh(self.bestdev, 
                              self.devs, 
                              gw, 
                              node.maproute.nip_to_ip(self.nodes[gw].firstnip), 
                              ntkd=self.nodes[gw])
                node.maproute.route_add(lvl, 
                                        nip[lvl], 
                                        neigh, 
                                        Rtt(100), # not used
                                        [], 
                                        silent=True)
                         
        # Partial global view of the routes (see the topology)
        # tuples of nodes like (src, dst, gw) where src and gw are ids,
        # and dst is a list of ids.        
        # NOTE: this topology is one-way only, i.e. each node 
        # has only one gateway to reach each other node,
        # thus we don't need to manage RTTs to discriminate routes
        routes = [ (1, [2, 3, 4, 5, 6], 3),
                   (2, [1, 3, 4, 5, 6], 1),
                   (3, [1, 2], 2),
                   (3, [4, 5, 6], 4),
                   (4, [1, 2, 3, 5, 6], 5),
                   (5, [1, 2, 3, 4, 6], 6),
                   (6, [1, 2, 3, 4, 5], 3)]
                   
        for (src, dst_list, gw) in routes:
            for dst in dst_list:
                add_routes(self.nodes[src], self.nodes[dst].firstnip, gw)
        
        def participant_add(node, service, nip):
            mp = service.mapp2p
            lvl = node.maproute.nip_cmp(nip, mp.me)
            for l in xrange(lvl, mp.levels):
                if not mp.node_get(l, nip[l]).participant:
                    mp.participant_node_add(l, nip[l])
                    
        for idn, node in self.nodes.items():
            node.counter.participate()
            node.andna.participate()
            
        # global view of the mapp2p (see the topology)
        # tuples of nodes like (learning_node, participant) where 
        # learning_node is an id, participant is a list of ids
        participant_map = [ (1, [2, 3, 4, 5, 6]),
                            (2, [1, 3, 4, 5, 6]),        
                            (3, [1, 2, 4, 5, 6]),
                            (4, [1, 2, 3, 5, 6]),
                            (5, [1, 2, 3, 4, 6]),
                            (6, [1, 2, 3, 4, 5])]
        
        for idn, participant_list in participant_map:
            for joining in participant_list:
                participant_add(self.nodes[idn], 
                                self.nodes[idn].andna, 
                                self.nodes[joining].firstnip)
                participant_add(self.nodes[idn], 
                                self.nodes[idn].counter, 
                                self.nodes[joining].firstnip)
        

    def tearDown(self):
        for idn, node in self.nodes.items():
            node_keys_path = self.keys_path + "/" + str(node.firstnip)
            files = os.listdir(node_keys_path)
            for f in files:
                os.remove(node_keys_path + "/" + f)
            os.rmdir(node_keys_path)
        
    def testRegistration(self, client=None, register_node=None,
                           hostname="Testing"):    
        if client is None:
            client = self.me
        if register_node is None:
            register_node = self.remote
        (res, data) = client.andna.register(hostname)
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
        self.testRegistration(self.me, self.remote, hostname)
        res, data = self.remote.andna.resolve(hostname)
        if res == 'OK':
            snsd_node = data
        self.failUnlessEqual(snsd_node.record == [12, 9, 8, 7], True)
        
    def testReverseResolution(self):
        local_cache_of_remote_host = None
        self.testRegistration()
        res, data = self.remote.andna.reverse_resolve([12, 9, 8, 7])
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
        self.me.andna.register(hostname="Test", 
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
            res, data = self.me.andna.register(hostname="hostname"+str(i))
            self.failIfEqual(i > limit and res == 'OK', True)
            if res != 'OK':
                self.failIfEqual('Hostnames limit reached' in str(data), False)
        
    def testMaxQueueHostname(self):
        hostname = "always the same"
        register_node_tmp = self.nodes[1].andna.H(self.nodes[1].andna.h(hostname))
        for i in xrange(1, 7):
            # the register node must be the same for all the client node
            register_node = self.nodes[i].andna.H(self.nodes[i].andna.h(hostname))
            self.failUnlessEqual(register_node_tmp == register_node, True)
            res, data = self.nodes[i].andna.register(hostname, append_if_unavailable=True)
            if i > self.me.andna.max_andna_queue:
                self.failIfEqual(res != 'OK' and 'enqueued' in str(data), False)
    
    def testHooking(self):
        # We don't use self.testRegistration, because the request is forwarded
        # into the gnode, we will directly write the entry into the cache 
        # of the neighbour 2, then the node 3 try to hook it and will 
        # take its cache
        self.nodes[2].andna.cache['taken by node 2'] = 'Fake AndnaCache value'
        # node 3 see node 2
        self.nodes[3].andna.neigh.send_event_neigh_new(self.bestdev, self.devs, 2, 
                    self.nodes[3].maproute.nip_to_ip(self.nodes[2].firstnip), 
                    netid=123, silent=1)
        # let's hook
        self.nodes[3].andna.p2pall.events.send('P2P_HOOKED', ())
        res = self.nodes[3].andna.cache.has_key('taken by node 2')
        self.failIfEqual(res, False)
                
    def testForwarding(self):
        hostname = "host"
        self.testRegistration(hostname=hostname)
        res = True
        for idn in range(1, 6):
            if self.nodes[idn].andna.cache.has_key(hostname) and \
               self.nodes[idn+1].andna.cache.has_key(hostname):
                res = self.nodes[idn].andna.cache[hostname] == self.nodes[idn+1].andna.cache[hostname]
                self.failIfEqual(res, False)
    
if __name__ == '__main__':
    unittest.main()