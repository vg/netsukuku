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

import datetime
import os
import sys
import time
import unittest

sys.path.append('..')

from network_simulator import create_nodes, create_network, create_service
from ntk.core.counter import CounterError
from ntk.lib.log import init_logger
from ntk.lib.micro import micro_block 
# TODO: resolve this... 
# please ignore the error messages regarding micro.py

init_logger()

# Topology of the network
#    1---2
#     \ /
#      3---6
#     /   /
#    4---5
                
class TestCounter(unittest.TestCase):
    
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
        
    def testPublicKeyHash(self, hostname="Testing hostname"):
        counter_gnode1 = self.me.counter.H(self.me.counter.h(hostname))
        counter_gnode2 = self.remote.counter.H(
                                        self.remote.counter.h(hostname))
        self.failUnlessEqual(counter_gnode1==counter_gnode2, True)
        self.failIfEqual(counter_gnode1 == self.me.firstnip, True)
        self.failIfEqual(counter_gnode1 == self.remote.firstnip, True)
        self.failIfEqual(counter_gnode2 == self.me.firstnip, True)
        self.failIfEqual(counter_gnode2 == self.remote.firstnip, True)
        
    def testUpdate(self, hostname="Testing hostname"):
        me_pubk   = self.me.andna.my_keys.get_pub_key()
        signature = self.me.andna.my_keys.sign(hostname)
        counter_gnode = self.me.counter.peer(key=hostname)
        res, data = counter_gnode.check(me_pubk, hostname, signature, 0)
        self.failUnlessEqual(res=='OK', True)
        for id in range(6,19):
            res, data = counter_gnode.check(me_pubk, hostname, signature, id)
            self.failUnlessEqual(res=='OK', False)            
        for id in range(1,10):
            res, data = counter_gnode.check(me_pubk, hostname, signature, id)
            self.failUnlessEqual(res=='OK', True)   
        
    def testHostnameHibernation(self):
        hostname = "Test"
        now = datetime.datetime.now()
        was = now - datetime.timedelta(days=self.remote.counter.
                                       expiration_days+1)
        expired_timestamp = time.mktime(was.timetuple())
        keys = self.me.andna.my_keys
        pub_k = keys.get_pub_key()
        signature = keys.sign(hostname)
        self.remote.counter.check(pub_k, hostname, signature, 0)
        # replace the timestamp field with the expired one
        self.remote.counter.cache[pub_k][hostname] = (expired_timestamp, 1)
        res = data = None
        try:
            res, data = self.remote.counter.check(pub_k, 
                                                  hostname, 
                                                  signature, 1)
        except CounterError, msg:
            self.failUnlessEqual('Expired' in str(msg), True)
        
    def testHostnameExpiration(self):
        hostname = "Test"
        now = datetime.datetime.now()
        was = now - datetime.timedelta(days=self.remote.counter.
                                       expiration_days*14)
        expired_timestamp = time.mktime(was.timetuple())
        keys = self.me.andna.my_keys
        pub_k = keys.get_pub_key()
        signature = keys.sign(hostname)
        self.remote.counter.check(pub_k, hostname, signature, 0)
        # replace the timestamp field with with the expired one
        self.remote.counter.cache[pub_k][hostname] = (expired_timestamp, 1)
        res = data = None
        try:
            res, data = self.remote.counter.check(pub_k, 
                                                  hostname, 
                                                  signature, 1)
        except CounterError, msg:
            self.failUnlessEqual('Expired hostname' in str(msg), True)
            
    def testHooking(self):
        # ignore ANDNA stuff...
        self.nodes[3].andna.p2pall.events.listeners['P2P_HOOKED'].pop(1)
        # We don't use self.testUpdate, because the request is forwarded
        # into the gnode, we will directly write the entry into the cache 
        # of the neighbour 2, then the node 3 try to hook it and will 
        # take its cache
        self.nodes[2].counter.cache['fake pubk'] = {}
        self.nodes[2].counter.cache['fake pubk']['taken by node 2'] = \
                                   'Fake values'
        # node 3 see node 2
        self.nodes[3].counter.neigh.send_event_neigh_new(self.bestdev, 
                    self.devs, 2, 
                    self.nodes[3].maproute.nip_to_ip(self.nodes[2].firstnip), 
                    netid=123, silent=1)
        # let's hook
        self.nodes[3].counter.p2pall.events.send('P2P_HOOKED', ())
        micro_block()
        res = self.nodes[3].counter.cache.has_key('fake pubk') and \
              self.nodes[3].counter.cache['fake pubk'].has_key('taken by node'
                                                               ' 2')
        self.failIfEqual(res, False)
    
    def testForwarding(self):
        hostname = "host"
        me_pubk  = self.me.counter.my_keys.get_pub_key()
        self.testUpdate(hostname=hostname)
        res = True
        for idn in xrange(5):
            if self.nodes[idn].counter.cache.has_key(me_pubk) and \
               self.nodes[idn+1].counter.cache.has_key(me_pubk) and \
               self.nodes[idn].counter.cache[me_pubk].has_key(hostname) and \
               self.nodes[idn+1].counter.cache[me_pubk].has_key(hostname):
                res = self.nodes[idn].counter.cache[me_pubk][hostname] == \
                      self.nodes[idn+1].counter.cache[me_pubk][hostname]
                self.failIfEqual(res, False)
                
if __name__ == '__main__':
    unittest.main()