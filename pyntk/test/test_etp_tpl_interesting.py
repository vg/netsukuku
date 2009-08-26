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
# Tests for use of TPL when processing ETPs
#
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
def neigh_add(ip, dev):
    pass
    #print 'add neighbour (ip, dev) ' + str((ip, dev))
def neigh_change(ip, dev):
    pass
    #print 'change neighbour (ip, dev) ' + str((ip, dev))
def neigh_delete(ip, dev):
    pass
    #print 'delete neighbour (ip, dev) ' + str((ip, dev))
def exec_rpc_call(ip, pieces, params):
    pass
    #print 'To ip ' + ip_to_str(ip) + ': ' + '.'.join(pieces) + str(params)

set_functions(_node_gonna_exec_etp=node_gonna_exec_etp,
              _nic_set_address=nic_set_address,
              _nic_init=nic_init,
              _route_add=route_add,
              _route_change=route_change,
              _route_delete=route_delete,
              _neigh_add=neigh_add,
              _neigh_change=neigh_change,
              _neigh_delete=neigh_delete,
              _delay_each_etp_exec=delay_each_etp_exec,
              _exec_rpc_call=exec_rpc_call)

def test_etp_newlink(link_is_really_good=False,
                     check_etp_emission=False,
                     check_y_routes=False):
    # In this test a new link X-B is detected, whose rem
    # is 50 if link_is_really_good else 150.
    #
    # if check_etp_emission:
    #     check that etp are emitted as expected.
    # elif check_y_routes:
    #     check in final results that, eventually, new rem to 1.* is
    #     discovered by Y.
    initialize()
    node_B = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1'], netid=12345)
    node_C = create_node(firstnip=[2,1,1,1], nics=['eth0', 'eth1'], netid=12345)
    node_D = create_node(firstnip=[3,1,1,1], nics=['eth0', 'eth1'], netid=12345)
    node_X = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1', 'eth2'], netid=12345)
    node_Y = create_node(firstnip=[3,3,3,3], nics=['eth0'], netid=12345)
    node_A = create_node(firstnip=[4,4,4,4], nics=['eth0', 'eth1'], netid=12345)
    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       X                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |             <void>                    C       |   #
    #   |--eth0   eth1         |                        |   #
    #   |       A              |-eth0   eth1------------|   #
    #                                 B                     #
    #########################################################
    # More clear drawing, but with no dev-names
    #########################################################
    #               200          100                        #
    #         D ---------- X ----------- Y                  #
    #         |            |                                #
    #      100|            |                                #
    #         |            |                                #
    #         C            |100                             #
    #         |            |                                #
    #      100|            |                                #
    #         |    50/150  |                                #
    #         B .......... A                                #
    #########################################################
    # A link B-A will appear, that iff is very good will change
    # the rem that Y has for NIP=1.*
    # Indeed B, C and D have NIP=1.*
    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_Y, node_X] = 1, 'eth0', 100
    nodelinks[node_X, node_Y] = 1, 'eth1', 100
    nodelinks[node_X, node_A] = 2, 'eth0', 100
    nodelinks[node_X, node_D] = 3, 'eth2', 200
    nodelinks[node_A, node_X] = 1, 'eth0', 100
    nodelinks[node_B, node_C] = 1, 'eth1', 100
    nodelinks[node_C, node_B] = 1, 'eth1', 100
    nodelinks[node_C, node_D] = 2, 'eth0', 100
    nodelinks[node_D, node_X] = 1, 'eth0', 200
    nodelinks[node_D, node_C] = 2, 'eth1', 100
    # future link
    nodelinks[node_A, node_B] = 2, 'eth1', (50 if link_is_really_good else 150)
    nodelinks[node_B, node_A] = 2, 'eth0', (50 if link_is_really_good else 150)
    ignore_etp_for=[]
    def node_x_meets_node_y(node_x,node_y):
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

    # link X - Y
    node_x_meets_node_y(node_X,node_Y)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link X - A
    node_x_meets_node_y(node_X,node_A)

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

    # link D - C
    node_x_meets_node_y(node_D,node_C)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link C - B
    node_x_meets_node_y(node_C,node_B)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'Verifying situation before link A-B appears.'
    msg = 'Wrong situation before link A-B appears'
    # Y has a route to 1.* through X rtt 300.
    gwnum,bestdev,rtt = nodelinks[node_Y, node_X]
    if node_Y.maproute.node_get(3, 1).route_getby_gw(gwnum).rem.value == 300: pass #print 'ok'
    else: return msg

    #########################################################
    #               200          100                        #
    #         D ---------- X ----------- Y                  #
    #         |            |                                #
    #      100|            |                                #
    #         |            |                                #
    #         C            |100                             #
    #         |            |                                #
    #      100|            |                                #
    #         |    50/150  |                                #
    #         B .......... A                                #
    #########################################################

    # link A - B
    node_x_meets_node_y(node_A,node_B)

    # Link A-B has appeared:

    if check_etp_emission:
        #print 'After link A-B appears... check ETP emitted'

        # Y has to receive an ETP from X iff link_is_really_good.
        _toY = False
        Y_lvl, Y_id = getcoords_node_x_in_map_of_y(node_Y, node_X)
        ret = 1
        while ret:
            for to_ip, etp_from_X in retrieve_etps_from_node(node_X):
                if to_ip == node_Y.maproute.nip_to_ip(node_Y.maproute.me):
                    _toY = True

            #retrieve etps and execute them in the proper node, but just once.
            ret = retrieve_execute_etps_once(exclude_ips=ignore_etp_for)
            simulate_delay()
            #repeat while there are ETPs.

        # Was there an ETP?
        if link_is_really_good:
            if _toY: pass #print 'ok'
            else: return 'Missing ETP from X to Y'
        else:
            if not _toY: pass #print 'ok'
            else: return 'Uninteresting ETP sent from X to Y'

    elif check_y_routes:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After link A-B appears... check  that, eventually, new rem to 1.* is discovered by Y.'
        msg = 'New rem to 1.* is not correctly discovered by Y.'
        # Y has a route to 1.* (through X, its only gateway)
        # with rtt 250 if link_is_really_good else 300.
        rem_expected = 250 if link_is_really_good else 300
        gwnum,bestdev,rtt = nodelinks[node_Y, node_X]
        if node_Y.maproute.node_get(3, 1).route_getby_gw(gwnum).rem.value == rem_expected: pass #print 'ok'
        else: return msg

    return False

class TestEtpTplInteresting(unittest.TestCase):

    def setUp(self):
        pass

    def test0101EtpNewLinkEtpEmission(self):
        '''Detecting a new link (check etp emission)'''
        msg = test_etp_newlink(check_etp_emission=True)
        if msg: self.fail(msg)

    def test0102EtpNewLinkNewRemDetected(self):
        '''Detecting a new link (check new REM is detected)'''
        msg = test_etp_newlink(check_y_routes=True)
        if msg: self.fail(msg)

    def test0201EtpNewLinkVeryGoodEtpEmission(self):
        '''Detecting a new very good link (check etp emission)'''
        msg = test_etp_newlink(link_is_really_good=True, check_etp_emission=True)
        if msg: self.fail(msg)

    def test0202EtpNewLinkVeryGoodNewRemDetected(self):
        '''Detecting a new very good link (check new REM is detected)'''
        msg = test_etp_newlink(link_is_really_good=True, check_y_routes=True)
        if msg: self.fail(msg)

if __name__ == '__main__':
    unittest.main()
