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

from andns.andns import AT_A, AndnsPacket, PROTO_UDP, NTK_REALM
from random import choice
import os
import sys
import unittest

sys.path.append('..')

from etp_simulator import create_node, initialize
from network_simulator import create_nodes, create_network, create_service
from ntk.config import settings
from ntk.core.andna import Andna, AndnaError
from ntk.core.andnswrapper import AndnsError
from ntk.core.counter import Counter
from ntk.core.p2p import P2PAll
from ntk.core.qspn import Etp
from ntk.core.route import MapRoute, Rtt
from ntk.core.radar import Neighbour, Neigh
from ntk.lib.log import init_logger
from ntk.lib.micro import micro_block
from ntk.wrap.xtime import swait
from ntk.network.inet import ip_to_str

init_logger()
initialize()

# Topology of the network
#    1---2
#     \ /
#      3---6
#     /   /
#    4---5

class TestAndns(unittest.TestCase):
    
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
            
    def testRegistration(self, client=None, hostname="Testing", service=0, IDNum=0, 
                         snsd_record='', snsd_record_pubk=None, priority=1, weight=1,
                         append_if_unavailable=False):    
        if client is None:
            client = self.me
        (res, data) = client.andna.register(hostname, service, IDNum, snsd_record,
                                            snsd_record_pubk, priority, weight,
                                            append_if_unavailable)
        if res == 'OK':
            timestamp, updates = data
        elif res == 'rmt_error':
            return res, AndnsError(data)
        return (res, (timestamp, updates))
    
    def testResolution(self):
        snsd_node = None
        hostname = "hostname for self.me"
        self.testRegistration(self.me, hostname, snsd_record='123.123.123.123')
        record = self.remote.andns.ntk_resolve(hostname, service=0, inverse=False)
        self.failUnlessEqual(record[0][4] == '123.123.123.123', True)
                
    def testProcess(self):
        self.testRegistration(self.remote, hostname="remote", snsd_record='123.123.123.123')
        request = AndnsPacket(id=4, r=1, qr=0, z=0, qtype=AT_A, ancount=0, ipv=0,
                                      nk=NTK_REALM, rcode=0, p=PROTO_UDP,
                                      service=0, qstdata="remote")
        response = self.me.andns.process_packet(request)
        for answer in response:
            self.failUnlessEqual(answer.rdata != "123.123.123.123", True)
        
    def testHooking(self):
        for idn, node in self.nodes.items():
            #node.p2p.events.send('P2P_HOOKED', ()) # equal to ...
            node.andns.update_hostnames()
        record = self.me.andns.ntk_resolve('node4', service=0, inverse=False)
        ip = ".".join(reversed([str(id) for id in self.nodes[4].firstnip]))
        self.failUnlessEqual(record[0][4] == ip, True)
    
if __name__ == '__main__':
    unittest.main()