##
# This file is part of Netsukuku
# (c) Copyright 2009 Luca Dionisi <luca.dionisi@gmail.com>
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
#
# Tests for ntk.core.route
#

import sys
import unittest
sys.path.append('..')

from ntk.lib.log import logger as logging
import ntk.core.radar as radar
import ntk.core.route as maproute
import ntk.core.qspn as qspn
import ntk.core.hook as hook
import ntk.core.p2p as p2p
import ntk.core.coord as coord
import ntk.core.krnl_route as kroute
import ntk.lib.rpc as rpc
import ntk.wrap.xtime as xtime
from ntk.lib.event import Event, apply_wakeup_on_event
from ntk.lib.micro import microfunc, micro, micro_block, allmicro_run

from ntk.config import settings, ImproperlyConfigured
from ntk.network import NICManager, Route
from ntk.network.inet import ip_to_str
from ntk.wrap.sock import Sock
from random import choice, randint

############################################
## for logging
from ntk.lib.log import init_logger
settings.VERBOSE_LEVEL = 3
settings.DEBUG_ON_SCREEN = True
init_logger()
##
############################################


############################################
## fakes:
##   network package:   NIC, Route
##   rpc stuff:   BcastClient, TCPClient
##   core stuff:   NtkNode, Radar, Neighbour

class FakeBcastClient:
    def __init__(self, nics):
        pass
rpc.BcastClient = FakeBcastClient

class FakeNtkNode:
    pass

import ntk.network
class FakeNIC:
    def up(self):
        print 'up'
    def down(self):
        print 'down'
    def show(self):
        print 'address ' + str(address)
    def set_address(self, address):
        print 'setting address ' + str(address)
        self.address = address
    def get_address(self):
        return self.address
    def filtering(self, *args, **kargs):
        print 'filtering ' + str(args) + str(kargs)
    def __init__(self, nic):
        print 'nic ' + nic
        self.nic = nic
        self.address = None
ntk.network.NIC = FakeNIC

class FakeRoute():
    @staticmethod
    def add(ip, cidr, dev=None, gateway=None):
        print 'add route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
    @staticmethod
    def change(ip, cidr, dev=None, gateway=None):
        print 'change route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None):
        print 'delete route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
    @staticmethod
    def flush():
        print 'flush'
    @staticmethod
    def flush_cache():
        print 'flush_cache'
    @staticmethod
    def ip_forward(enable):
        print 'ip_forward (enable) ' + str(enable)
import ntk.core.krnl_route
ntk.network.Route = FakeRoute
ntk.core.krnl_route.KRoute = FakeRoute

class Null:
    def __init__(self, concat):
        self.concat = concat
    def __getattr__(self, name):
        return Null(str(self.concat) + '.' + str(name))
    def __call__(self, *params):
        print str(self.concat) + str(params)
        return True
    
class FakeTCPClient:
    def __init__(self, ip):
        self.ip = ip
    def __getattr__(self, name):
        return Null('To ip ' + str(self.ip) + ': ' + str(name))
    
class FakeNeighbour():
    def __init__(self):
        real_ = radar.Neighbour(16)
        self.events = Event(['NEIGH_NEW', 'NEIGH_DELETED', 'NEIGH_REM_CHGED'])
        self.dict_id_to_neigh = {}
        self.announce_gw = real_.announce_gw
        self.waitfor_gw_added = real_.waitfor_gw_added
        self.announce_gw_added = real_.announce_gw_added
        self.announce_gw_removing = real_.announce_gw_removing
        self.waitfor_gw_removable = real_.waitfor_gw_removable
        self.announce_gw_removable = real_.announce_gw_removable
    def set_netid(self, *args):
        pass
    def ip_to_neigh(self, ip):
        for key, neigh in self.dict_id_to_neigh.items():
            if neigh.ip == ip: return neigh
        return None
    def id_to_neigh(self, idn):
        return self.dict_id_to_neigh[idn]
    def neigh_list(self):
        """ return the list of neighbours """
        nlist = []
        for key, neigh in self.dict_id_to_neigh.items():
            nlist.append(neigh)
        return nlist
    def readvertise(self):
        for key, neigh in self.dict_id_to_neigh.items():
            self.send_event_neigh_new_for_neigh(neigh)
    def send_event_neigh_new_for_neigh(self, neigh):
        self.dict_id_to_neigh[neigh.id] = neigh
        logging.debug('ANNOUNCE: gw ' + str(neigh.id) + ' detected.')
        self.announce_gw(neigh.id)
        self.events.send('NEIGH_NEW', (neigh,))
    def send_event_neigh_new(self, bestdev,devs,idn,ip,netid):
        neigh = radar.Neigh(bestdev=bestdev,
                                        devs=devs,
                                        idn=idn,
                                        ip=ip,
                                        netid=netid,
                                        ntkd=FakeTCPClient(ip))
        self.send_event_neigh_new_for_neigh(neigh)
    def send_event_neigh_rem_chged(self, bestdev,devs,idn,ip,netid,new_rtt):
        self.events.send('NEIGH_REM_CHGED',
                                 (radar.Neigh(bestdev=bestdev,
                                        devs=devs,
                                        idn=idn,
                                        ip=ip,
                                        netid=netid,
                                        ntkd=FakeTCPClient(ip)),Rtt(new_rtt)))
    def send_event_neigh_deleted(self, idn,ip,netid):
        logging.debug('ANNOUNCE: gw ' + str(idn) + ' removing.')
        self.announce_gw_removing(idn)
        self.events.send('NEIGH_DELETED',
                         (radar.Neigh(bestdev=None,
                                devs=None,
                                idn=idn,
                                ip=ip,
                                netid=netid,
                                ntkd=FakeTCPClient(ip)),))
        if idn in self.dict_id_to_neigh: self.dict_id_to_neigh.pop(idn)
class FakeRadar():
    def __init__(self, ntkd, rpcbcastclient, xtime):
        self.neigh = FakeNeighbour()
radar.Radar = FakeRadar

##
########################################


#######################################
##  Each simulated node has an instance of FakeNtkNode
##   created with the function create_node
gsize, levels = 256, 4
def create_node(firstnip, nics, netid):
    node = FakeNtkNode()
    node.nic_manager = NICManager(nics=nics)
    rpcbcastclient = rpc.BcastClient(list(node.nic_manager))
    node.radar = radar.Radar(node, rpcbcastclient, xtime)
    node.neighbour = node.radar.neigh
    node.firstnip = firstnip
    node.maproute = maproute.MapRoute(node, levels, gsize, node.firstnip)
    node.etp = qspn.Etp(node, node.radar, node.maproute)
    node.p2p = p2p.P2PAll(node, node.radar, node.maproute, node.etp)
    node.coordnode = coord.Coord(node, node.radar, node.maproute, node.p2p)
    node.hook = hook.Hook(node, node.radar, node.maproute, node.etp,
                                 node.coordnode, node.nic_manager)
    node.kroute = kroute.KrnlRoute(node, node.neighbour, node.maproute)
    node.neighbour.netid = netid
    
    # we add some code before a etp forward
    node.etp.etp_forward_ = node.etp.etp_forward
    def fakeetpforward(_self, etp, exclude):
        print 'ETP forwarding ' + str(etp)
        _self.etp_forward_(etp, exclude)
    node.etp.etp_forward = fakeetpforward

    return node

##
########################################


#######################################
##  Remember to add a little delay  (eg xtime.swait(3000))
##  after each simulated signal or event.
##  This will provide enough micro_block to completely
##  execute each started microthread.

node_guest1 = create_node(firstnip=[4,3,2,1], nics=['eth0', 'eth1'], netid=12345)
node_guest2 = create_node(firstnip=[8,7,6,5], nics=['eth0', 'eth1'], netid=12345)

# a little delay will provide 
xtime.swait(3000)

# node_guest2 is a new neighbour to node_guest1
node_guest1.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_guest1.maproute.nip_to_ip(node_guest2.maproute.me),
                      node_guest2.neighbour.netid)
xtime.swait(3000)

## an etp received
#R = [[],[],[],[(9, maproute.Rtt(100))]]
#TPL = [[0, [[8, maproute.NullRem()]]]]
#node_guest1.etp.etp_exec([8,7,6,5], node_guest1.neighbour.netid, R, TPL, 1)
#xtime.swait(3000)
#
## a new neighbour
#node_guest1.neighbour.send_event_neigh_new(('eth1',100),{'eth1':100},2,node_guest1.maproute.nip_to_ip([16,15,14,13]),node_guest1.neighbour.netid)
#xtime.swait(3000)
#
## an etp received (will be discarded for acyclic rule...)
#R = [[],[],[],[]]
#TPL = [[3, [[1, maproute.NullRem()], [13, maproute.Rtt(100)], [9, maproute.Rtt(100)]]], [0, [[8, maproute.Rtt(100)]]]]
#node_guest1.etp.etp_exec([8,7,6,5], node_guest1.neighbour.netid, R, TPL, 1)
#xtime.swait(3000)
#
## an etp received
#R = [[],[],[],[(5, maproute.Rtt(100))]]
#TPL = [[3, [[9, maproute.NullRem()]]], [0, [[16, maproute.Rtt(100)]]]]
#node_guest1.etp.etp_exec([16,15,14,13], node_guest1.neighbour.netid, R, TPL, 1)
#xtime.swait(3000)
#
### remove a neighbour
#node_guest1.neighbour.send_event_neigh_deleted(1,node_guest1.maproute.nip_to_ip([8,7,6,5]),node_guest1.neighbour.netid)
#xtime.swait(300)

xtime.swait(1000000000)
allmicro_run()

class TestLucaRoute(unittest.TestCase):

    def setUp(self):
        pass

    def testFake(self):
        ''' Fake test'''
        pass

if __name__ == '__main__':
    unittest.main()
