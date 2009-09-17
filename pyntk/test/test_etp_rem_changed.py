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
# Tests for propagation of information with ETP when the rem of a link
# changes.
#

import sys
import unittest

sys.path.append('..')

# Iff it is suitable, use this to work-around the functions that are defined
# as microfunc. Otherwise, use a suitable value in the "simulate_delay" and
# "delay_each_etp_exec" parts.
import functools
def fakemicrofunc(is_micro=False):
    def decorate(func):
        @functools.wraps(func)
        def fmicro(*data, **kwargs):
            func(*data, **kwargs)
        return fmicro
    return decorate
import ntk.lib.micro
ntk.lib.micro.microfunc = fakemicrofunc

import ntk.wrap.xtime as xtime
from ntk.lib.micro import microfunc, micro, micro_block, allmicro_run
from ntk.network.inet import ip_to_str
from ntk.config import settings
import ntk.core.route as maproute

from etp_simulator import initialize
from etp_simulator import create_node
from etp_simulator import node_x_in_map_of_y, getcoords_node_x_in_map_of_y
from etp_simulator import retrieve_execute_etps_once, retrieve_execute_etps
from etp_simulator import simulated_nodes, remove_simulated_node_by_ip
from etp_simulator import set_functions, retrieve_etps_to_ip
from etp_simulator import retrieve_etps_from_node

#######################################
##  Remember to add a little delay  (eg simulate_delay())
##  after each simulated signal or event.
##  This will provide enough micro_block to completely
##  execute each started microthread.
def simulate_delay():
    xtime.swait(0)

############################################
## for logging
from ntk.lib.log import init_logger
settings.VERBOSE_LEVEL = 1
settings.DEBUG_ON_SCREEN = True
init_logger()

def log_executing_node(node):
    ip = node.maproute.nip_to_ip(node.maproute.me)
    ipstr = ip_to_str(ip)
    print 'Logs from now on are from node ' + ipstr

def printmaps():
    for node in simulated_nodes:
        log_executing_node(node)
        print node.maproute.repr_me()

def node_gonna_exec_etp(node):
    pass
    #log_executing_node(node)
def delay_each_etp_exec():
    return 0
def nic_set_address(the_NIC, address):
    pass
    #print 'setting address ' + str(address)
def nic_init(the_NIC, nic):
    pass
    #print 'nic ' + nic
def route_add(ip, cidr, dev=None, gateway=None):
    pass
    #print 'add route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def route_change(ip, cidr, dev=None, gateway=None):
    pass
    #print 'change route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def route_delete(ip, cidr, dev=None, gateway=None):
    pass
    #print 'delete route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def exec_rpc_call(ip, pieces, params):
    pass
    #print 'To ip ' + ip_to_str(ip) + ': ' + '.'.join(pieces) + str(params)

set_functions(_node_gonna_exec_etp=node_gonna_exec_etp,
              _nic_set_address=nic_set_address,
              _nic_init=nic_init,
              _route_add=route_add,
              _route_change=route_change,
              _route_delete=route_delete,
              _delay_each_etp_exec=delay_each_etp_exec,
              _exec_rpc_call=exec_rpc_call)

def test_etp_remchanged():
    # In this test The link X-Y will change its rem in 50.
    initialize()
    node_D = create_node(firstnip=[1,1,1,1], nics=['eth0'], netid=12345)
    node_X = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1'], netid=12345)
    node_Y = create_node(firstnip=[3,3,3,3], nics=['eth0', 'eth1'], netid=12345)
    node_A = create_node(firstnip=[4,4,4,4], nics=['eth0'], netid=12345)
    #########################################################
    #         100              100              100         #
    #  D ------------- X --------------- Y ------------- A  #
    #    eth0     eth0   eth1       eth0   eth1      eth0   #
    #########################################################
    # The link X-Y will change its rem in 50
    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_D, node_X] = 1, 'eth0', 100
    nodelinks[node_X, node_D] = 1, 'eth0', 100
    nodelinks[node_X, node_Y] = 2, 'eth1', 100
    nodelinks[node_Y, node_X] = 1, 'eth0', 100
    nodelinks[node_Y, node_A] = 2, 'eth1', 100
    nodelinks[node_A, node_Y] = 1, 'eth0', 100
    ignore_etp_for=[]
    def node_x_meets_node_y(node_x,node_y,viceversa=True):
        # node_y is a new neighbour to node_x
        #log_executing_node(node_x)
        gwnum,bestdev,rtt = nodelinks[node_x, node_y]
        node_x.neighbour.send_event_neigh_new(
        	              (bestdev,rtt),
        	              {bestdev:rtt},
        	              gwnum,
        	              node_x.maproute.nip_to_ip(node_y.maproute.me),
        	              node_y.neighbour.netid)
        simulate_delay()
        if viceversa:
            # node_x is a new neighbour to node_y
            #log_executing_node(node_y)
            gwnum,bestdev,rtt = nodelinks[node_y, node_x]
            node_y.neighbour.send_event_neigh_new(
        	              (bestdev,rtt),
        	              {bestdev:rtt},
        	              gwnum,
        	              node_y.maproute.nip_to_ip(node_x.maproute.me),
        	              node_x.neighbour.netid)
            simulate_delay()

    def link_changes_between_node_x_and_node_y(node_x, node_y, new_rtt, viceversa=True):
        # node_x detects
        #log_executing_node(node_x)
        gwnum,bestdev,old_rtt = nodelinks[node_x, node_y]
        nodelinks[node_x, node_y] = gwnum,bestdev,new_rtt
        node_x.neighbour.send_event_neigh_rem_chged(
                              (bestdev,new_rtt),
        	              {bestdev:new_rtt},
        	              gwnum,
        	              node_x.maproute.nip_to_ip(node_y.maproute.me),
        	              node_y.neighbour.netid,
        	              old_rtt)
        simulate_delay()
        if viceversa:
            # node_y detects
            #log_executing_node(node_y)
            gwnum,bestdev,old_rtt = nodelinks[node_y, node_x]
            nodelinks[node_y, node_x] = gwnum,bestdev,new_rtt
            node_y.neighbour.send_event_neigh_rem_chged(
                              (bestdev,new_rtt),
        	              {bestdev:new_rtt},
        	              gwnum,
        	              node_y.maproute.nip_to_ip(node_x.maproute.me),
        	              node_x.neighbour.netid,
        	              old_rtt)
            simulate_delay()

    # link X - Y
    node_x_meets_node_y(node_X,node_Y)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link Y - A
    node_x_meets_node_y(node_Y,node_A)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link X - D
    node_x_meets_node_y(node_X,node_D)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'Verifying situation before link changes.'
    msg = 'Wrong situation before link changes'
    # D has just 1 route to A, rtt 300.
    if len(node_x_in_map_of_y(node_A, node_D).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_A, node_D).best_route().rem.value == 300: pass #print 'ok'
    else: return msg
    # A has just 1 route to D, rtt 300.
    if len(node_x_in_map_of_y(node_A, node_D).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_A, node_D).best_route().rem.value == 300: pass #print 'ok'
    else: return msg

    # X detects change
    link_changes_between_node_x_and_node_y(node_X,node_Y,50,viceversa=False)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # Y detects change
    link_changes_between_node_x_and_node_y(node_Y,node_X,50,viceversa=False)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'After changes A..D has to be 250 for both.
    msg = 'A has wrong rems'
    # A has just 1 route to D, rtt 250.
    if len(node_x_in_map_of_y(node_A, node_D).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_A, node_D).best_route().rem.value == 250: pass #print 'ok'
    else: return msg
    msg = 'D has wrong rems'
    # D has just 1 route to A, rtt 250.
    if len(node_x_in_map_of_y(node_A, node_D).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_A, node_D).best_route().rem.value == 250: pass #print 'ok'
    else: return msg

    return False

def test_etp_remchanged_nodeleaf():
    # In this test The link X-Y will change its rem in 50.
    initialize()
    node_X = create_node(firstnip=[1,1,1,1], nics=['eth0'], netid=12345)
    node_Y = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1'], netid=12345)
    node_A = create_node(firstnip=[3,3,3,3], nics=['eth0'], netid=12345)
    #########################################################
    #                          100              100         #
    #                  X --------------- Y ------------- A  #
    #                    eth0       eth0   eth1      eth0   #
    #########################################################
    # The link X-Y will change its rem in 50
    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_X, node_Y] = 1, 'eth0', 100
    nodelinks[node_Y, node_X] = 1, 'eth0', 100
    nodelinks[node_Y, node_A] = 2, 'eth1', 100
    nodelinks[node_A, node_Y] = 1, 'eth0', 100
    ignore_etp_for=[]
    def node_x_meets_node_y(node_x,node_y,viceversa=True):
        # node_y is a new neighbour to node_x
        #log_executing_node(node_x)
        gwnum,bestdev,rtt = nodelinks[node_x, node_y]
        node_x.neighbour.send_event_neigh_new(
        	              (bestdev,rtt),
        	              {bestdev:rtt},
        	              gwnum,
        	              node_x.maproute.nip_to_ip(node_y.maproute.me),
        	              node_y.neighbour.netid)
        simulate_delay()
        if viceversa:
            # node_x is a new neighbour to node_y
            #log_executing_node(node_y)
            gwnum,bestdev,rtt = nodelinks[node_y, node_x]
            node_y.neighbour.send_event_neigh_new(
        	              (bestdev,rtt),
        	              {bestdev:rtt},
        	              gwnum,
        	              node_y.maproute.nip_to_ip(node_x.maproute.me),
        	              node_x.neighbour.netid)
            simulate_delay()

    def link_changes_between_node_x_and_node_y(node_x, node_y, new_rtt, viceversa=True):
        # node_x detects
        #log_executing_node(node_x)
        gwnum,bestdev,old_rtt = nodelinks[node_x, node_y]
        nodelinks[node_x, node_y] = gwnum,bestdev,new_rtt
        node_x.neighbour.send_event_neigh_rem_chged(
                              (bestdev,new_rtt),
        	              {bestdev:new_rtt},
        	              gwnum,
        	              node_x.maproute.nip_to_ip(node_y.maproute.me),
        	              node_y.neighbour.netid,
        	              old_rtt)
        simulate_delay()
        if viceversa:
            # node_y detects
            #log_executing_node(node_y)
            gwnum,bestdev,old_rtt = nodelinks[node_y, node_x]
            nodelinks[node_y, node_x] = gwnum,bestdev,new_rtt
            node_y.neighbour.send_event_neigh_rem_chged(
                              (bestdev,new_rtt),
        	              {bestdev:new_rtt},
        	              gwnum,
        	              node_y.maproute.nip_to_ip(node_x.maproute.me),
        	              node_x.neighbour.netid,
        	              old_rtt)
            simulate_delay()

    # link X - Y
    node_x_meets_node_y(node_X,node_Y)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link Y - A
    node_x_meets_node_y(node_Y,node_A)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'Verifying situation before link changes.'
    msg = 'Wrong situation before link changes'
    # A has just 1 route to X, rtt 200.
    if len(node_x_in_map_of_y(node_X, node_A).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_X, node_A).best_route().rem.value == 200: pass #print 'ok'
    else: return msg

    # X and Y detect change
    link_changes_between_node_x_and_node_y(node_X,node_Y,50)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'After changes X..A has to be 150 for both.
    msg = 'X has wrong rems'
    # X has just 1 route to A, rtt 150.
    if len(node_x_in_map_of_y(node_A, node_X).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_A, node_X).best_route().rem.value == 150: pass #print 'ok'
    else: return msg
    msg = 'A has wrong rems'
    # A has just 1 route to X, rtt 150.
    if len(node_x_in_map_of_y(node_X, node_A).routes) == 1: pass #print 'ok'
    else: return msg
    if node_x_in_map_of_y(node_X, node_A).best_route().rem.value == 150: pass #print 'ok'
    else: return msg

    return False

class TestEtpTplInteresting(unittest.TestCase):

    def setUp(self):
        pass

    def test01EtpRemChanged(self):
        '''Detecting a changed rem link (check final result)'''
        msg = test_etp_remchanged()
        if msg: self.fail(msg)

    def test02EtpRemChangedNodeLeaf(self):
        '''Detecting a changed rem link to a leaf node (check final result)'''
        msg = test_etp_remchanged_nodeleaf()
        if msg: self.fail(msg)

if __name__ == '__main__':
    unittest.main()
