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
# Tests for use of exclude_gw when sending ETPs
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

def test_etp_newlink(link_is_really_good=False,
                     check_etp_emission=False,
                     check_new_rem_detected=False,
                     check_cyclic_routes=False,
                     check_tpl_misleading=False,
                     check_all_routes_stored=False):
    # In this test a new link N-M is detected, whose rem
    # is 50 if link_is_really_good else 150.
    #
    # if check_etp_emission:
    #     check that etp are emitted as expected.
    # elif check_new_rem_detected:
    #     check in final results that, eventually, new rem to N is
    #     discovered by Y.
    # elif check_cyclic_routes:
    #     check in final results that cyclic routes are not found.
    # elif check_tpl_misleading:
    #     check in final results that TPL part has not been misleading to Y
    #     for best routes to M and D that were already known.
    # elif check_all_routes_stored:
    #     check in final results that all routes (not just best ones)
    #     have been stored.
    initialize()
    node_C = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1', 'eth2'], netid=12345)
    node_D = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1'], netid=12345)
    node_N = create_node(firstnip=[3,3,3,3], nics=['eth0', 'eth1'], netid=12345)
    node_M = create_node(firstnip=[4,4,4,4], nics=['eth0', 'eth1'], netid=12345)
    node_Y = create_node(firstnip=[5,5,5,5], nics=['eth0'], netid=12345)
    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       C                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |                                       N       |   #
    #   |                                               |   #
    #   |--eth0   eth1 <void>                           |   #
    #   |       M                                       |   #
    #########################################################
    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_M, node_C] = 1, 'eth0', 100
    nodelinks[node_Y, node_C] = 1, 'eth0', 100
    nodelinks[node_C, node_M] = 1, 'eth0', 100
    nodelinks[node_C, node_Y] = 2, 'eth1', 100
    nodelinks[node_C, node_D] = 3, 'eth2', 100
    nodelinks[node_D, node_C] = 1, 'eth0', 100
    nodelinks[node_D, node_N] = 2, 'eth1', 100
    nodelinks[node_N, node_D] = 1, 'eth0', 100
    # future link
    nodelinks[node_N, node_M] = 2, 'eth1', (50 if link_is_really_good else 150)
    nodelinks[node_M, node_N] = 2, 'eth1', (50 if link_is_really_good else 150)
    ignore_etp_for=[]

    # node_C is a new neighbour to node_M
    #log_executing_node(node_M)
    gwnum,bestdev,rtt = nodelinks[node_M, node_C]
    node_M.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_M.maproute.nip_to_ip(node_C.maproute.me),
    	              node_C.neighbour.netid)
    simulate_delay()

    # node_M is a new neighbour to node_C
    #log_executing_node(node_C)
    gwnum,bestdev,rtt = nodelinks[node_C, node_M]
    node_C.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_C.maproute.nip_to_ip(node_M.maproute.me),
    	              node_M.neighbour.netid)
    simulate_delay()

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       C                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |                                       N       |   #
    #   |                                               |   #
    #   |--eth0   eth1 <void>                           |   #
    #   |       M                                       |   #
    #########################################################

    # node_C is a new neighbour to node_Y
    #log_executing_node(node_Y)
    gwnum,bestdev,rtt = nodelinks[node_Y, node_C]
    node_Y.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_Y.maproute.nip_to_ip(node_C.maproute.me),
    	              node_C.neighbour.netid)
    simulate_delay()

    # node_Y is a new neighbour to node_C
    #log_executing_node(node_C)
    gwnum,bestdev,rtt = nodelinks[node_C, node_Y]
    node_C.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_C.maproute.nip_to_ip(node_Y.maproute.me),
    	              node_Y.neighbour.netid)
    simulate_delay()

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       C                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |                                       N       |   #
    #   |                                               |   #
    #   |--eth0   eth1 <void>                           |   #
    #   |       M                                       |   #
    #########################################################

    # node_C is a new neighbour to node_D
    #log_executing_node(node_D)
    gwnum,bestdev,rtt = nodelinks[node_D, node_C]
    node_D.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_D.maproute.nip_to_ip(node_C.maproute.me),
    	              node_C.neighbour.netid)
    simulate_delay()

    # node_D is a new neighbour to node_C
    #log_executing_node(node_C)
    gwnum,bestdev,rtt = nodelinks[node_C, node_D]
    node_C.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_C.maproute.nip_to_ip(node_D.maproute.me),
    	              node_D.neighbour.netid)
    simulate_delay()

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       C                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |                                       N       |   #
    #   |                                               |   #
    #   |--eth0   eth1 <void>                           |   #
    #   |       M                                       |   #
    #########################################################

    # node_N is a new neighbour to node_D
    #log_executing_node(node_D)
    gwnum,bestdev,rtt = nodelinks[node_D, node_N]
    node_D.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_D.maproute.nip_to_ip(node_N.maproute.me),
    	              node_N.neighbour.netid)
    simulate_delay()

    # node_D is a new neighbour to node_N
    #log_executing_node(node_N)
    gwnum,bestdev,rtt = nodelinks[node_N, node_D]
    node_N.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_N.maproute.nip_to_ip(node_D.maproute.me),
    	              node_D.neighbour.netid)
    simulate_delay()

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'Verifying situation before link N-M appears.'
    msg = 'Wrong situation before link N-M appears'
    # C has a route to N through D rtt 200.
    gwnum,bestdev,rtt = nodelinks[node_C, node_D]
    if node_x_in_map_of_y(node_N, node_C).route_getby_gw(gwnum).rem.value == 200: pass #print 'ok'
    else: return msg

    #########################################################
    #   |               |--eth0                             #
    #   |               |       Y                           #
    #   |--eth0   eth1--|                                   #
    #   |       C                                           #
    #   |         eth2--|                                   #
    #   |               |--eth0   eth1--|                   #
    #   |                       D       |                   #
    #   |                               |--eth0   eth1--|   #
    #   |                                       N       |   #
    #   |                                               |   #
    #   |--eth0   eth1----------------------------------|   #
    #   |       M                                       |   #
    #########################################################

    # node_N is a new neighbour to node_M
    #log_executing_node(node_M)
    gwnum,bestdev,rtt = nodelinks[node_M, node_N]
    node_M.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_M.maproute.nip_to_ip(node_N.maproute.me),
    	              node_N.neighbour.netid)
    simulate_delay()

    # node_M is a new neighbour to node_N
    #log_executing_node(node_N)
    gwnum,bestdev,rtt = nodelinks[node_N, node_M]
    node_N.neighbour.send_event_neigh_new(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
    	              node_N.maproute.nip_to_ip(node_M.maproute.me),
    	              node_M.neighbour.netid)
    simulate_delay()

    # Link M-N has appeared:

    if check_etp_emission:
        #print 'After link M-N appears... check ETP emitted'

        # N has to tell to M new routes for (N itself in the TPL) D, C, Y.

        _toD = False
        _toC = False
        _toY = False
        # Where are D, C, Y in N's map?
        D_lvl, D_id = getcoords_node_x_in_map_of_y(node_D, node_N)
        C_lvl, C_id = getcoords_node_x_in_map_of_y(node_C, node_N)
        Y_lvl, Y_id = getcoords_node_x_in_map_of_y(node_Y, node_N)
        for to_ip, etp_from_N in retrieve_etps_from_node(node_N):
            if to_ip == node_M.maproute.nip_to_ip(node_M.maproute.me):
                R = etp_from_N[2]
                for r1 in R[D_lvl]:
                    if r1[0] == D_id: _toD = True
                for r1 in R[C_lvl]:
                    if r1[0] == C_id: _toC = True
                for r1 in R[Y_lvl]:
                    if r1[0] == Y_id: _toY = True
        if _toD and _toC and _toY: pass #print 'ok'
        else: return 'Wrong ETP from N to M'

        #retrieve etps and execute them in the proper node, but just once.
        retrieve_execute_etps_once(exclude_ips=ignore_etp_for)
        simulate_delay()

        # Now, M has to tell to C new routes for (M itself, N in the TPL) D, Y.

        _toD = False
        _toY = False
        # Where are D, Y in M's map?
        D_lvl, D_id = getcoords_node_x_in_map_of_y(node_D, node_M)
        Y_lvl, Y_id = getcoords_node_x_in_map_of_y(node_Y, node_M)
        for to_ip, etp_from_M in retrieve_etps_from_node(node_M):
            if to_ip == node_C.maproute.nip_to_ip(node_C.maproute.me):
                R = etp_from_M[2]
                for r1 in R[D_lvl]:
                    if r1[0] == D_id: _toD = True
                for r1 in R[Y_lvl]:
                    if r1[0] == Y_id: _toY = True
        if _toD and _toY: pass #print 'ok'
        else: return 'Wrong ETP from M to C'

        #retrieve etps and execute them in the proper node, but just once.
        retrieve_execute_etps_once(exclude_ips=ignore_etp_for)
        simulate_delay()

        # Now, C has to tell to D new route for N (in the TPL)

        _etp_toD = False
        for to_ip, etp_from_C in retrieve_etps_from_node(node_C):
            if to_ip == node_D.maproute.nip_to_ip(node_D.maproute.me):
                _etp_toD = True
        if _etp_toD: pass #print 'ok'
        else: return 'Missing ETP from C to D'

        # Also, C has to tell to Y new REM for route for N iff link
        #  CM..N is better than CD..N
        _etp_toY = False
        for to_ip, etp_from_C in retrieve_etps_from_node(node_C):
            if to_ip == node_Y.maproute.nip_to_ip(node_Y.maproute.me):
                _etp_toY = True
        if link_is_really_good:
            if _etp_toY: pass #print 'ok'
            else: return 'Missing ETP from C to Y'
        else:
            if not _etp_toY: pass #print 'ok'
            else: return 'Uninteresting ETP sent from C to Y'

    elif check_new_rem_detected:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After link N-M appears... check  that, eventually, new rem to N is discovered by Y.'
        msg = 'New rem to N is not correctly discovered by Y.'
        # Y has a route to N (through C, its only gateway)
        # with rtt 250 if link_is_really_good else 300.
        rem_expected = 250 if link_is_really_good else 300
        if node_x_in_map_of_y(node_N, node_Y).best_route().rem.value == rem_expected: pass #print 'ok'
        else: return msg

    elif check_cyclic_routes:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After link N-M appears... check that cyclic routes are not found.'
        msg = 'Cyclic routes have been saved by C.'
        # C has only 1 route to Y.
        if len(node_x_in_map_of_y(node_Y, node_C).routes) == 1: pass #print 'ok'
        else: return msg

    elif check_tpl_misleading:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After link N-M appears... check that TPL part has not been misleading to Y.'
        msg = 'Y has uncorrectly updated rem for some route.'
        # Y has a route to D (through C, its only gateway) with rtt 200.
        if node_x_in_map_of_y(node_D, node_Y).best_route().rem.value == 200: pass #print 'ok'
        else: return msg
        # Y has a route to M (through C, its only gateway) with rtt 200.
        if node_x_in_map_of_y(node_M, node_Y).best_route().rem.value == 200: pass #print 'ok'
        else: return msg

    elif check_all_routes_stored:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After link N-M appears... check that all routes have been stored.'
        msg = 'Not all routes have been stored.'
        # C has 2 routes to D, M, N.
        if len(node_x_in_map_of_y(node_D, node_C).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_M, node_C).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_N, node_C).routes) == 2: pass #print 'ok'
        else: return msg
        # D has 2 routes to C, M, N.
        if len(node_x_in_map_of_y(node_C, node_D).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_M, node_D).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_N, node_D).routes) == 2: pass #print 'ok'
        else: return msg
        # M has 2 routes to C, D, N.
        if len(node_x_in_map_of_y(node_C, node_M).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_D, node_M).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_N, node_M).routes) == 2: pass #print 'ok'
        else: return msg
        # N has 2 routes to C, D, M.
        if len(node_x_in_map_of_y(node_C, node_N).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_D, node_N).routes) == 2: pass #print 'ok'
        else: return msg
        if len(node_x_in_map_of_y(node_M, node_N).routes) == 2: pass #print 'ok'
        else: return msg

    return False

class TestETPExcludeGateways(unittest.TestCase):

    def setUp(self):
        pass
    # In this test a new link N-M is detected, whose rem
    # is 50 if link_is_really_good else 150.
    #
    # if check_etp_emission:
    #     check that etp are emitted as expected.
    # elif check_new_rem_detected:
    #     check in final results that, eventually, new rem to N is
    #     discovered by Y.
    # elif check_cyclic_routes:
    #     check in final results that cyclic routes are not found.
    # elif check_tpl_misleading:
    #     check in final results that TPL part has not been misleading to Y
    #     for best routes to M and D that were already known.
    # elif check_all_routes_stored:
    #     check in final results that all routes (not just best ones)
    #     have been stored.

    def test0101EtpNewLinkEtpEmission(self):
        '''Detecting a new link (check etp emission)'''
        msg = test_etp_newlink(check_etp_emission=True)
        if msg: self.fail(msg)

    def test0102EtpNewLinkNewRemDetected(self):
        '''Detecting a new link (check new REM is detected)'''
        msg = test_etp_newlink(check_new_rem_detected=True)
        if msg: self.fail(msg)

    def test0103EtpNewLinkCyclicRoutes(self):
        '''Detecting a new link (check cyclic routes not stored)'''
        msg = test_etp_newlink(check_cyclic_routes=True)
        if msg: self.fail(msg)

    def test0104EtpNewLinkTPL(self):
        '''Detecting a new link (check tpl not misleading)'''
        msg = test_etp_newlink(check_tpl_misleading=True)
        if msg: self.fail(msg)

    def test0105EtpNewLinkAllRoutes(self):
        '''Detecting a new link (check all routes stored)'''
        msg = test_etp_newlink(check_all_routes_stored=True)
        if msg: self.fail(msg)

    def test0201EtpNewLinkVeryGoodEtpEmission(self):
        '''Detecting a new very good link (check etp emission)'''
        msg = test_etp_newlink(link_is_really_good=True, check_etp_emission=True)
        if msg: self.fail(msg)

    def test0202EtpNewLinkVeryGoodNewRemDetected(self):
        '''Detecting a new very good link (check new REM is detected)'''
        msg = test_etp_newlink(link_is_really_good=True, check_new_rem_detected=True)
        if msg: self.fail(msg)

    def test0203EtpNewLinkVeryGoodCyclicRoutes(self):
        '''Detecting a new very good link (check cyclic routes not stored)'''
        msg = test_etp_newlink(link_is_really_good=True, check_cyclic_routes=True)
        if msg: self.fail(msg)

    def test0204EtpNewLinkVeryGoodTPL(self):
        '''Detecting a new very good link (check tpl not misleading)'''
        msg = test_etp_newlink(link_is_really_good=True, check_tpl_misleading=True)
        if msg: self.fail(msg)

    def test0205EtpNewLinkVeryGoodAllRoutes(self):
        '''Detecting a new very good link (check all routes stored)'''
        msg = test_etp_newlink(link_is_really_good=True, check_all_routes_stored=True)
        if msg: self.fail(msg)

if __name__ == '__main__':
    unittest.main()
