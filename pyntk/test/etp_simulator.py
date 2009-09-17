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
# Tests for synchronization of ip commands, ETP execution, and so on...
#

import sys
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
from ntk.lib.rencode import loads, dumps


##
def fnull(*args):
    pass
def default_delay_each_etp_exec():
    return 800
def set_functions(_node_gonna_exec_etp=fnull,
                  _nic_set_address=fnull,
                  _nic_init=fnull,
                  _route_add=fnull,
                  _route_change=fnull,
                  _route_delete=fnull,
                  _delay_each_etp_exec=default_delay_each_etp_exec,
                  _exec_rpc_call=fnull):
    global node_gonna_exec_etp
    node_gonna_exec_etp = _node_gonna_exec_etp
    global nic_set_address
    nic_set_address = _nic_set_address
    global nic_init
    nic_init = _nic_init
    global route_add
    route_add = _route_add
    global route_change
    route_change = _route_change
    global route_delete
    route_delete = _route_delete
    global delay_each_etp_exec
    delay_each_etp_exec = _delay_each_etp_exec
    global exec_rpc_call
    exec_rpc_call = _exec_rpc_call
set_functions()

############################################
## For storing ETPs.
## We simulate the sending by intercepting calls to
## neigh.ntkd.etp.etp_exec (see FakeTCPClient)
class EtpData:
    def __init__(self, etp):
        self.sender_nip = etp[0]
        self.sender_netid = etp[1]
        self.R = etp[2]
        self.TPL = etp[3]
        self.flag_of_interest = etp[4]
class EtpPool:
    def __init__(self):
        self.etps = []
    def add(self, to_ip, etp):
        self.etps.append((to_ip, loads(dumps(etp))))
    def get_ips(self):
        return [x[0] for x in self.etps]
    def get_first_etp_by_ip(self, ip, remove=False):
        for index in xrange(len(self.etps)):
            etp_pair = self.etps[index]
            if etp_pair[0] == ip:
                if remove: self.etps[index:index+1] = []
                return etp_pair[1]
        return None
    def pop_first_etp_by_ip(self, ip):
        return self.get_first_etp_by_ip(ip, remove=True)
    def get_etps_to_ip(self, ip):
        ret = []
        for index in xrange(len(self.etps)):
            etp_pair = self.etps[index]
            if etp_pair[0] == ip:
                ret.append(etp_pair[1])
        return ret
    def get_etps_from_node(self, node):
        ret = []
        for index in xrange(len(self.etps)):
            etp_pair = self.etps[index]
            etp = etp_pair[1]
            if EtpData(etp).sender_nip == node.maproute.me:
                ret.append((etp_pair[0], etp)) # (to_ip, etp)
        return ret
etp_pool = EtpPool()
##
############################################


############################################
## fakes:
##   network package:   NIC, Route
##   rpc stuff:   BcastClient, TCPClient
##   core stuff:   NtkNode, Radar, Neighbour, Hook.communicating_vessels

class FakeBcastClient:
    def __init__(self, nics):
        pass
rpc.BcastClient = FakeBcastClient

class FakeNtkNode:
    pass

import ntk.network
class FakeNIC:
    def up(self):
        pass #print 'up'
    def down(self):
        pass #print 'down'
    def show(self):
        pass #print 'address ' + str(address)
    def set_address(self, address):
        nic_set_address(self, address)
        self.address = address
    def get_address(self):
        return self.address
    def filtering(self, *args, **kargs):
        pass #print 'filtering ' + str(args) + str(kargs)
    def __init__(self, nic):
        nic_init(self, nic)
        self.nic = nic
        self.address = None
ntk.network.NIC = FakeNIC

class FakeRoute():
    @staticmethod
    def add(ip, cidr, dev=None, gateway=None):
        route_add(ip, cidr, dev, gateway)
    @staticmethod
    def change(ip, cidr, dev=None, gateway=None):
        route_add(ip, cidr, dev, gateway)
    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None):
        route_add(ip, cidr, dev, gateway)
    @staticmethod
    def flush():
        pass #print 'flush'
    @staticmethod
    def flush_cache():
        pass #print 'flush_cache'
    @staticmethod
    def ip_forward(enable):
        pass #print 'ip_forward (enable) ' + str(enable)
import ntk.core.krnl_route
ntk.network.Route = FakeRoute
ntk.core.krnl_route.KRoute = FakeRoute

class Null:
    def __init__(self, ip, pieces=[]):
        self.ip = ip
        self.pieces = pieces
    def __getattr__(self, name):
        return Null(self.ip, self.pieces + [name])
    def __call__(self, *params):
        exec_rpc_call(self.ip, self.pieces, params)
        if 'etp' in self.pieces and 'etp_exec' in self.pieces: etp_pool.add(self.ip, params)
        return True

class FakeTCPClient:
    def __init__(self, ip):
        self.ip = ip
    def __getattr__(self, name):
        return Null(self.ip, [name])

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
    def id_to_ip(self, idn):
        return self.dict_id_to_neigh[idn].ip
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
    def send_event_neigh_new(self, bestdev, devs, idn, ip, netid):
        neigh = radar.Neigh(bestdev=bestdev,
                                        devs=devs,
                                        idn=idn,
                                        ip=ip,
                                        netid=netid,
                                        ntkd=FakeTCPClient(ip))
        self.send_event_neigh_new_for_neigh(neigh)
        return neigh
    def send_event_neigh_rem_chged(self, bestdev, devs, idn, ip, netid, new_rtt):
        self.events.send('NEIGH_REM_CHGED',
                                 (radar.Neigh(bestdev=bestdev,
                                        devs=devs,
                                        idn=idn,
                                        ip=ip,
                                        netid=netid,
                                        ntkd=FakeTCPClient(ip)),Rtt(new_rtt)))
    def send_event_neigh_deleted(self, bestdev, devs, idn, ip, netid):
        logging.debug('ANNOUNCE: gw ' + str(idn) + ' removing.')
        self.announce_gw_removing(idn)
        self.events.send('NEIGH_DELETED',
                         (radar.Neigh(bestdev=bestdev,
                                devs=devs,
                                idn=idn,
                                ip=ip,
                                netid=netid,
                                ntkd=FakeTCPClient(ip)),))
        if idn in self.dict_id_to_neigh: self.dict_id_to_neigh.pop(idn)
class FakeRadar():
    def __init__(self, ntkd, rpcbcastclient, xtime):
        self.neigh = FakeNeighbour()
radar.Radar = FakeRadar

def fake_com_vessels(*args):
    pass
hook.Hook.communicating_vessels = fake_com_vessels

##
########################################


#######################################
##  Each simulated node has an instance of FakeNtkNode
##   created with the function create_node.
##  When a node dies use the function remove_simulated_node_by_ip.
##  We have a list of all current nodes (simulated_nodes).
##  We have a method to retrieve a node by its ip
##   (get_simulated_node_by_ip).
gsize, levels = 256, 4
simulated_nodes = []
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

    simulated_nodes.append(node)
    return node
def get_simulated_node_by_ip(ip):
    for node in simulated_nodes:
        if node.maproute.nip_to_ip(node.maproute.me) == ip:
            return node
    return None
def remove_simulated_node_by_ip(ip):
    node = get_simulated_node_by_ip(ip)
    if node: simulated_nodes.remove(node)
##  We have a method to obtain the pair (lvl, id) where node_x is saved
##   in node_y's map.
def getcoords_node_x_in_map_of_y(node_x, node_y):
    x_nip = node_x.maproute.me
    y_nip = node_y.maproute.me
    lvl = node_y.maproute.nip_cmp(y_nip, x_nip)
    return (lvl, x_nip[lvl])
##  We have a method to find node_x as a destination (RouteNode)
##   in node_y's map.
def node_x_in_map_of_y(node_x, node_y):
    return node_y.maproute.node_get(
                  *getcoords_node_x_in_map_of_y(node_x, node_y) )
##  We have a method to retrieve etps and execute them in the proper node
##   just once
def retrieve_execute_etps_once(exclude_ips=[]):
    tot = 0
    etp_ips =  etp_pool.get_ips()
    for ip in etp_ips:
        tot += 1
        if ip in exclude_ips:
            etp_pool.pop_first_etp_by_ip(ip)
        else:
            etp = etp_pool.pop_first_etp_by_ip(ip)
            node = get_simulated_node_by_ip(ip)
            if node:
                node_gonna_exec_etp(node)
                node.etp.etp_exec(*etp)
                xtime.swait(delay_each_etp_exec())
    return tot
##  We have a method to *repeatedly* retrieve and execute etps until the end
def retrieve_execute_etps(exclude_ips=[]):
    tot = 0
    etp_ips =  etp_pool.get_ips()
    while len(etp_ips) > 0:
        tot += retrieve_execute_etps_once(exclude_ips=exclude_ips)
        etp_ips = etp_pool.get_ips()
    return tot
##  We have a method to retrieve all etps in the pool given a destination ip
def retrieve_etps_to_ip(ip):
    return etp_pool.get_etps_to_ip(ip)
##  We have a method to retrieve all etps in the pool given a source node
def retrieve_etps_from_node(node):
    return etp_pool.get_etps_from_node(node)
##
##  We provide a method to clear all things before a new test.
def initialize():
    simulated_nodes[:] = []
    global etp_pool
    etp_pool = EtpPool()
##
########################################



