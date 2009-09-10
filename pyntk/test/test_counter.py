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
from random import choice
import os
import sys
import time
import unittest

sys.path.append('..')

from etp_simulator import create_node
from ntk.core.counter import Counter, CounterError
from ntk.core.p2p import P2PAll
from ntk.core.qspn import Etp
from ntk.core.route import MapRoute, Rtt
from ntk.core.radar import Neighbour, Neigh
from ntk.lib.crypto import KeyPair
from ntk.lib.log import init_logger

init_logger()

# reused Eriol's code from test_p2p.py

class FakeRadar(object):

    def __init__(self):
        self.neigh = Neighbour()

class LittleTestCounter(unittest.TestCase):
    
    def setUp(self):
        self.keys_path = os.getcwd() + "/keys/"
        if not os.path.exists(self.keys_path):
            os.mkdir(self.keys_path)
        radar = FakeRadar()
        maproute = MapRoute(ntkd=None, levels=4, gsize=256, me=[8, 19, 82, 84])
        etp = Etp(ntkd=None, radar=radar, maproute=maproute)
        p2pall = P2PAll(ntkd=None, radar=radar, maproute=maproute, etp=etp)
        self.keypair = KeyPair(keys_path=self.keys_path+"keypair")
        self.counter = Counter(radar, maproute, p2pall, self.keypair)
        for i in xrange(4):
            self.counter.check(self.keypair.get_pub_key(), 
                               'hostname', 
                               self.keypair.sign('hostname'), 
                               i)

    def testValidCheck(self):
        for i in xrange(4, 14):
            res, data = self.counter.check(self.keypair.get_pub_key(), 
                                           'hostname', 
                                           self.keypair.sign('hostname'), 
                                           i)
            if res == 'OK':
                self.failUnlessEqual(data[1]>0, True) 
                return
        self.fail()
            
    def testFailedCheck(self):
        res = data = 0
        for i in xrange(3):
            try:
                res, data = self.counter.check('pubk', 
                                               'hostname', 
                                               'nosign', 
                                               i)
            except:
                self.failUnlessEqual(res, 0)
            if res == 'OK':
                self.failUnlessEqual(data[1]>0, False)
    

# Topology of the network
#    1---2
#     \ /
#      3---6
#     /   /
#    4---5
# reused lukisi's code from etp_simulator.py 
# (TODO: create_node function has been modified a bit)
                
class TestCounter(unittest.TestCase):
    
    def setUp(self):
        self.keys_path = os.getcwd() + "/keys"
        self.nics = ['This', 'are', 'just', 'few', 'fake', 'interfaces']
        self.devs = {}
        for nic in self.nics:
            self.devs[nic] = 10 # fake average value
        self.bestdev = ('fake', 10)
        self.nodes = {} # item = idn: FakeNtkNode
        for idn in range(1, 7):
            if not os.path.exists(self.keys_path):
                os.mkdir(self.keys_path)
            node_keys_path = self.keys_path + "/" + str([idn+11, 9, 8, 7])
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
                         
        # partial global view of the routes (see the topology)
        # tuples of nodes like (src, dst, gw) where src and gw are ids,
        # and dst is a list of ids
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
                                self.nodes[idn].counter, 
                                self.nodes[joining].firstnip)

    def tearDown(self):
        for idn, node in self.nodes.items():
            node_keys_path = self.keys_path + "/" + str(node.firstnip)
            files = os.listdir(node_keys_path)
            for f in files:
                os.remove(node_keys_path + "/" + f)
            os.rmdir(node_keys_path)
        
    def testPublicKeyHash(self, hostname="Testing hostname"):
        counter_gnode1 = self.me.counter.H(self.me.counter.h(hostname))
        counter_gnode2 = self.remote.counter.H(self.remote.counter.h(hostname))
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
        # ignore ANDNA stuff
        self.nodes[3].andna.p2pall.events.listeners['P2P_HOOKED'].pop(1)
        # We don't use self.testUpdate, because the request is forwarded
        # into the gnode, we will directly write the entry into the cache 
        # of the neighbour 2, then the node 3 try to hook it and will 
        # take its cache
        self.nodes[2].counter.cache['fake pubk'] = {}
        self.nodes[2].counter.cache['fake pubk']['taken by node 2'] = 'Fake values'
        # node 3 see node 2
        self.nodes[3].counter.neigh.send_event_neigh_new(self.bestdev, self.devs, 2, 
                    self.nodes[3].maproute.nip_to_ip(self.nodes[2].firstnip), 
                    netid=123, silent=1)
        # let's hook
        self.nodes[3].counter.p2pall.events.send('P2P_HOOKED', ())
        res = self.nodes[3].counter.cache.has_key('fake pubk') and \
              self.nodes[3].counter.cache['fake pubk'].has_key('taken by node 2')
        self.failIfEqual(res, False)
    
    def testForwarding(self):
        hostname = "host"
        me_pubk  = self.me.counter.my_keys.get_pub_key()
        self.testUpdate(hostname=hostname)
        res = True
        for idn in range(1, 6):
            if self.nodes[idn].counter.cache.has_key(me_pubk) and \
               self.nodes[idn+1].counter.cache.has_key(me_pubk) and \
               self.nodes[idn].counter.cache[me_pubk].has_key(hostname) and \
               self.nodes[idn+1].counter.cache[me_pubk].has_key(hostname):
                res = self.nodes[idn].counter.cache[me_pubk][hostname] == \
                      self.nodes[idn+1].counter.cache[me_pubk][hostname]
                self.failIfEqual(res, False)
                
if __name__ == '__main__':
    unittest.main()