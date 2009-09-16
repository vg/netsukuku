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
# Tests for ETP execution and forwarding with many nodes in a collision
# domain.
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

def test_etp_new_changed(check_etp_emission=True):
    # check_etp_emission: true  => check etp are emitted as expected.
    #                              don't check final results.
    # check_etp_emission: false => don't check etp are emitted as expected.
    #                              check final results.
    initialize()
    node_A = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1'], netid=12345)
    node_B = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1'], netid=12345)
    node_C = create_node(firstnip=[3,3,3,3], nics=['eth0'], netid=12345)
    node_D = create_node(firstnip=[4,4,4,4], nics=['eth0', 'eth1'], netid=12345)
    node_E = create_node(firstnip=[5,5,5,5], nics=['eth0', 'eth1'], netid=12345)
    #########################################################
    #   |--eth1   eth0--|                  /^^^^^^^^\       #
    #   |       A       |--eth0   eth1--|--<  12.*  >       #
    #   |               |       B          <  13.*  >       #
    #   |                                  \VVVVVVVV/       #
    #   |--eth0                                             #
    #   |       C                      /^^^^^^^^\           #
    #   |                              <  12.*  >           #
    #   |--eth0   eth1--|--------------<  14.*  >           #
    #   |       D                      \VVVVVVVV/           #
    #   |                                                   #
    #   |--eth0   eth1--<void>                              #
    #   |       E                                           #
    #########################################################

    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_A, node_B] = 1, 'eth0', 100
    nodelinks[node_B, node_A] = 1, 'eth0', 100
    nodelinks[node_A, node_D] = 2, 'eth1', 100
    nodelinks[node_D, node_A] = 1, 'eth0', 100
    nodelinks[node_A, node_E] = 3, 'eth1', 100
    nodelinks[node_D, node_E] = 2, 'eth0', 100
    nodelinks[node_E, node_A] = 1, 'eth0', 100
    nodelinks[node_E, node_D] = 2, 'eth0', 100
    nodelinks[node_A, node_C] = 4, 'eth1', 100
    nodelinks[node_D, node_C] = 3, 'eth0', 100
    nodelinks[node_E, node_C] = 3, 'eth0', 100
    nodelinks[node_C, node_A] = 1, 'eth0', 100
    nodelinks[node_C, node_D] = 2, 'eth0', 100
    nodelinks[node_C, node_E] = 3, 'eth0', 100
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

    # node_B has a neighbour (the number 10) with ip 101.102.103.104
    # through which he sees:
    #    gnode 12 of level 3 at Rtt 3456;
    #    gnode 13 of level 3 at Rtt 3456;
    #log_executing_node(node_B)
    node_B.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_B.maproute.nip_to_ip([104,103,102,101]),
    	              12345)
    simulate_delay()
    node_B.maproute.route_add(3, 12, 10, maproute.Rtt(3456))
    simulate_delay()
    node_B.maproute.route_add(3, 13, 10, maproute.Rtt(3456))
    simulate_delay()
    ignore_etp_for.append(node_B.maproute.nip_to_ip([104,103,102,101]))

    # node_D has a neighbour (the number 10) with ip 111.112.113.114
    # through which he sees gnode 12 of level 3 at Rtt 5678.
    # through which he sees:
    #    gnode 12 of level 3 at Rtt 5678;
    #    gnode 14 of level 3 at Rtt 200;
    #log_executing_node(node_D)
    node_D.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_D.maproute.nip_to_ip([114,113,112,111]),
    	              12345)
    simulate_delay()
    node_D.maproute.route_add(3, 12, 10, maproute.Rtt(5678))
    simulate_delay()
    node_D.maproute.route_add(3, 14, 10, maproute.Rtt(200))
    simulate_delay()
    ignore_etp_for.append(node_D.maproute.nip_to_ip([114,113,112,111]))

    # link A - B
    node_x_meets_node_y(node_A,node_B)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link A - D
    node_x_meets_node_y(node_A,node_D)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link A - E
    node_x_meets_node_y(node_A,node_E)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link D - E
    node_x_meets_node_y(node_D,node_E)

    #retrieve etps and execute them in the proper node
    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #print 'Verifying situation before node C appears.'
    msg = 'Wrong situation before C appears'
    # A has routes to 12.* through B rtt 3556.
    gwnum,bestdev,rtt = nodelinks[node_A, node_B]
    if node_A.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 3556: pass #print 'ok'
    else: return msg
    # A has routes to 13.* through B rtt 3556.
    gwnum,bestdev,rtt = nodelinks[node_A, node_B]
    if node_A.maproute.node_get(3, 13).route_getby_gw(gwnum).rem.value == 3556: pass #print 'ok'
    else: return msg
    # A has routes to 12.* through D rtt 5778.
    gwnum,bestdev,rtt = nodelinks[node_A, node_D]
    if node_A.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 5778: pass #print 'ok'
    else: return msg
    # A has routes to 14.* through D rtt 300.
    gwnum,bestdev,rtt = nodelinks[node_A, node_D]
    if node_A.maproute.node_get(3, 14).route_getby_gw(gwnum).rem.value == 300: pass #print 'ok'
    else: return msg
    # E has routes to 12.* through A rtt 3656.
    gwnum,bestdev,rtt = nodelinks[node_E, node_A]
    if node_E.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 3656: pass #print 'ok'
    else: return msg
    # E has routes to 13.* through A rtt 3656.
    gwnum,bestdev,rtt = nodelinks[node_E, node_A]
    if node_E.maproute.node_get(3, 13).route_getby_gw(gwnum).rem.value == 3656: pass #print 'ok'
    else: return msg
    # E has routes to 12.* through D rtt 5778.
    gwnum,bestdev,rtt = nodelinks[node_E, node_D]
    if node_E.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 5778: pass #print 'ok'
    else: return msg
    # E has routes to 14.* through D rtt 300.
    gwnum,bestdev,rtt = nodelinks[node_E, node_D]
    if node_E.maproute.node_get(3, 14).route_getby_gw(gwnum).rem.value == 300: pass #print 'ok'
    else: return msg
    # D has routes to 12.* through A rtt 3656. It's its best route.
    gwnum,bestdev,rtt = nodelinks[node_D, node_A]
    if node_D.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 3656: pass #print 'ok'
    else: return msg
    if node_D.maproute.node_get(3, 12).best_route() is node_D.maproute.node_get(3, 12).route_getby_gw(gwnum): pass #print 'ok'
    else: return msg
    # It has another route too.
    if len(node_D.maproute.node_get(3, 12).routes) > 1: pass #print 'ok'
    else: return msg

    #########################################################
    #   |--eth1   eth0--|                  /^^^^^^^^\       #
    #   |       A       |--eth0   eth1--|--<  12.*  >       #
    #   |               |       B          <  13.*  >       #
    #   |                                  \VVVVVVVV/       #
    #   |--eth0                                             #
    #   |       C                      /^^^^^^^^\           #
    #   |                              <  12.*  >           #
    #   |--eth0   eth1--|--------------<  14.*  >           #
    #   |       D                      \VVVVVVVV/           #
    #   |                                                   #
    #   |--eth0   eth1--<void>                              #
    #   |       E                                           #
    #########################################################


    # link C - A
    node_x_meets_node_y(node_C,node_A)

    # link C - D
    node_x_meets_node_y(node_C,node_D)

    # link C - E
    node_x_meets_node_y(node_C,node_E)


    # Node C has appeared:

    if check_etp_emission:
        #print 'After node C appears... check ETP emitted'

        # A has routes to 3,12 through B.
        # A has routes to 3,12 through D.
        # A has routes to 3,13 through B.
        # A has routes to 3,14 through D.
        # A will tell to C it can be used to get to 12 and 13.
        # A won't tell to C it can be used to get to 14.

        _to12 = False
        _to13 = False
        _to14 = False
        for to_ip, etp_from_A in retrieve_etps_from_node(node_A):
            if to_ip == node_C.maproute.nip_to_ip(node_C.maproute.me):
                R = etp_from_A[2]
                for r1 in R[3]:
                    if r1[0] == 12: _to12 = True
                    if r1[0] == 13: _to13 = True
                    if r1[0] == 14: _to14 = True
        if _to12 and _to13 and not _to14: pass #print 'ok'
        else: return 'Wrong ETP from A to C'

        # E has routes to 3,12 through A.
        # E has routes to 3,12 through D.
        # E has routes to 3,13 through A.
        # E has routes to 3,14 through D.
        # E won't tell to C any route through it.

        _anydest = False
        for to_ip, etp_from_E in retrieve_etps_from_node(node_E):
            if to_ip == node_C.maproute.nip_to_ip(node_C.maproute.me):
                R = etp_from_E[2]
                for r1 in R[3]:
                    _anydest = True
        if not _anydest: pass #print 'ok'
        else: return 'Wrong ETP from E to C'

        # D has routes to 3,12 through A.
        # D has routes to 3,13 through A.
        # D has routes to 3,12 through another node.
        # D has routes to 3,14 through another node.
        # D will tell to C it can be used to get to 12 and 14.
        #   ... even though D prefers to use A to go to 12.
        # D won't tell to C it can be used to get to 13.

        _to12 = False
        _to13 = False
        _to14 = False
        for to_ip, etp_from_D in retrieve_etps_from_node(node_D):
            if to_ip == node_C.maproute.nip_to_ip(node_C.maproute.me):
                R = etp_from_D[2]
                for r1 in R[3]:
                    if r1[0] == 12: _to12 = True
                    if r1[0] == 13: _to13 = True
                    if r1[0] == 14: _to14 = True
        if _to12 and _to14 and not _to13: pass #print 'ok'
        else: return 'Wrong ETP from D to C'

    else:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After node C appears... check final situation'
        msg = 'Wrong final situation'
        # C has routes to 12.* through A rtt 3656. It's its best route.
        gwnum,bestdev,rtt = nodelinks[node_C, node_A]
        if node_C.maproute.node_get(3, 12).route_getby_gw(gwnum).rem.value == 3656: pass #print 'ok'
        else: return msg
        if node_C.maproute.node_get(3, 12).best_route() is node_C.maproute.node_get(3, 12).route_getby_gw(gwnum): pass #print 'ok'
        else: return msg
        # It has another route too.
        if len(node_C.maproute.node_get(3, 12).routes) == 2: pass #print 'ok'
        else: return msg
        # C has only one route to 13 and to 14.
        if len(node_C.maproute.node_get(3, 13).routes) == 1: pass #print 'ok'
        else: return msg
        if len(node_C.maproute.node_get(3, 14).routes) == 1: pass #print 'ok'
        else: return msg

    return False

def test_etp_new_dead(check_etp_emission=True):
    # check_etp_emission: true  => check etp are emitted as expected.
    #                              don't check final results.
    # check_etp_emission: false => don't check etp are emitted as expected.
    #                              check final results.
    initialize()
    node_X = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1'], netid=12345)
    node_W = create_node(firstnip=[2,2,2,2], nics=['eth0'], netid=12345)
    node_B = create_node(firstnip=[3,3,3,3], nics=['eth0', 'eth1'], netid=12345)
    node_Y = create_node(firstnip=[4,4,4,4], nics=['eth0'], netid=12345)
    node_Z = create_node(firstnip=[5,5,5,5], nics=['eth0'], netid=12345)
    #####################################################################
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                X       |               |       Y                  #
    #                        |--eth0   eth1--|                          #
    #                        |       B       |                          #
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                W       |               |       Z                  #
    #####################################################################
    nodelinks={}
    # nodelinks[(node_self,node_other)] = (gwnum,bestdev,rtt)
    nodelinks[node_X, node_W] = 1, 'eth0', 100
    nodelinks[node_X, node_B] = 2, 'eth0', 100
    nodelinks[node_W, node_X] = 1, 'eth0', 100
    nodelinks[node_W, node_B] = 2, 'eth0', 100
    nodelinks[node_B, node_X] = 1, 'eth0', 100
    nodelinks[node_B, node_W] = 2, 'eth0', 100
    nodelinks[node_B, node_Y] = 3, 'eth1', 100
    nodelinks[node_B, node_Z] = 4, 'eth1', 100
    nodelinks[node_Y, node_Z] = 1, 'eth0', 100
    nodelinks[node_Y, node_B] = 2, 'eth0', 100
    nodelinks[node_Z, node_Y] = 1, 'eth0', 100
    nodelinks[node_Z, node_B] = 2, 'eth0', 100
    simulate_delay()
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

    # node_W has a neighbour (the number 10) with ip 101.102.103.104
    # through which he sees:
    #    gnode 42 of level 3 at Rtt 2900;
    #log_executing_node(node_W)
    node_W.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_W.maproute.nip_to_ip([104,103,102,101]),
    	              12345)
    simulate_delay()
    node_W.maproute.route_add(3, 42, 10, maproute.Rtt(2900))
    simulate_delay()
    ignore_etp_for.append(node_W.maproute.nip_to_ip([104,103,102,101]))

    # node_X has a neighbour (the number 10) with ip 111.102.103.104
    # through which he sees:
    #    gnode 42 of level 3 at Rtt 900;
    #log_executing_node(node_X)
    node_X.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_X.maproute.nip_to_ip([104,103,102,111]),
    	              12345)
    simulate_delay()
    node_X.maproute.route_add(3, 42, 10, maproute.Rtt(900))
    simulate_delay()
    ignore_etp_for.append(node_X.maproute.nip_to_ip([104,103,102,111]))

    # node_Y has a neighbour (the number 10) with ip 121.102.103.104
    # through which he sees:
    #    gnode 42 of level 3 at Rtt 1900;
    #log_executing_node(node_Y)
    node_Y.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_Y.maproute.nip_to_ip([104,103,102,121]),
    	              12345)
    simulate_delay()
    node_Y.maproute.route_add(3, 42, 10, maproute.Rtt(1900))
    simulate_delay()
    ignore_etp_for.append(node_Y.maproute.nip_to_ip([104,103,102,121]))
    node_Z
    # node_Z has a neighbour (the number 10) with ip 131.102.103.104
    # through which he sees:
    #    gnode 42 of level 3 at Rtt 1900;
    #log_executing_node(node_Z)
    node_Z.neighbour.send_event_neigh_new(
    	              ('eth1',100),
    	              {'eth1':100},
    	              10,
    	              node_Z.maproute.nip_to_ip([104,103,102,131]),
    	              12345)
    simulate_delay()
    node_Z.maproute.route_add(3, 42, 10, maproute.Rtt(1900))
    simulate_delay()
    ignore_etp_for.append(node_Z.maproute.nip_to_ip([104,103,102,131]))


    # link X - B
    node_x_meets_node_y(node_X,node_B)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link W - B
    node_x_meets_node_y(node_W,node_B)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link W - X
    node_x_meets_node_y(node_W,node_X)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link Y - B
    node_x_meets_node_y(node_Y,node_B)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link Z - B
    node_x_meets_node_y(node_Z,node_B)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    # link Y - Z
    node_x_meets_node_y(node_Y,node_Z)

    retrieve_execute_etps(exclude_ips=ignore_etp_for)
    #printmaps()
    simulate_delay()

    #####################################################################
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                X       |               |       Y                  #
    #                        |--eth0   eth1--|                          #
    #                        |       B       |                          #
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                W       |               |       Z                  #
    #####################################################################

    #print 'Verifying situation before node X dies.'
    msg = 'Wrong situation before node X dies'
    # B has 4 routes towards 42.*.
    if len(node_B.maproute.node_get(3, 42).routes) == 4: pass #print 'ok'
    else: return msg
    # B through X towards 42.* has rtt 1000.
    gwnum,bestdev,rtt = nodelinks[node_B, node_X]
    if node_B.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 1000: pass #print 'ok'
    else: return msg
    # B through W towards 42.* has rtt 3000.
    gwnum,bestdev,rtt = nodelinks[node_B, node_W]
    if node_B.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 3000: pass #print 'ok'
    else: return msg
    # B through Y towards 42.* has rtt 2000.
    gwnum,bestdev,rtt = nodelinks[node_B, node_Y]
    if node_B.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 2000: pass #print 'ok'
    else: return msg
    # B through Z towards 42.* has rtt 2000.
    gwnum,bestdev,rtt = nodelinks[node_B, node_Z]
    if node_B.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 2000: pass #print 'ok'
    else: return msg
    # X has 3 routes towards 42.*.
    if len(node_X.maproute.node_get(3, 42).routes) == 3: pass #print 'ok'
    else: return msg
    # W has 3 routes towards 42.*.
    if len(node_W.maproute.node_get(3, 42).routes) == 3: pass #print 'ok'
    else: return msg
    # Y has 3 routes towards 42.*.
    if len(node_Y.maproute.node_get(3, 42).routes) == 3: pass #print 'ok'
    else: return msg
    # Z has 3 routes towards 42.*.
    if len(node_Z.maproute.node_get(3, 42).routes) == 3: pass #print 'ok'
    else: return msg
    # Z through B towards 42.* has rtt 1100.
    gwnum,bestdev,rtt = nodelinks[node_Z, node_B]
    if node_Z.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 1100: pass #print 'ok'
    else: return msg

    ## node_X dies
    node_X_nip = node_X.maproute.me
    node_X_ip = node_X.maproute.nip_to_ip(node_X_nip)
    node_X_netid = node_X.neighbour.netid
    remove_simulated_node_by_ip(node_X_ip)
    ## node_B detects it
    #log_executing_node(node_B)
    gwnum,bestdev,rtt = nodelinks[node_B, node_X]
    node_B.neighbour.send_event_neigh_deleted(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
                          node_X_ip,
                          node_X_netid)
    simulate_delay()
    ## node_W detects it too
    #log_executing_node(node_W)
    gwnum,bestdev,rtt = nodelinks[node_W, node_X]
    node_W.neighbour.send_event_neigh_deleted(
    	              (bestdev,rtt),
    	              {bestdev:rtt},
    	              gwnum,
                          node_X_ip,
                          node_X_netid)
    simulate_delay()

    #####################################################################
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                X       |               |       Y                  #
    #                        |--eth0   eth1--|                          #
    #                        |       B       |                          #
    #                        |               |                          #
    # < 42.* >--eth1   eth0--|               |--eth0   eth1--< 42.* >   #
    #                W       |               |       Z                  #
    #####################################################################

    # Node X died:

    if check_etp_emission:
        #print 'After node X dies... check ETP emitted'

        # B has nothing to say to W.

        _toW = False
        for to_ip, etp_from_B in retrieve_etps_from_node(node_B):
            if to_ip == node_W.maproute.nip_to_ip(node_W.maproute.me):
                _toW = True
        if not _toW: pass #print 'ok'
        else: return 'B should say nothing to W'

        # B has something to say to Z.
        # Its best route to 42, excluding Z itself,
        # and excluding Y cause Z reaches Y by itself,
        # is (through W) with rtt 3000.

        _toZ = False
        _to42 = False
        _to42is3000 = False
        for to_ip, etp_from_B in retrieve_etps_from_node(node_B):
            if to_ip == node_Z.maproute.nip_to_ip(node_Z.maproute.me):
                _toZ = True
                R = etp_from_B[2]
                for r1 in R[3]:
                    if r1[0] == 42:
                        _to42 = True
                        if r1[1].value == 3000:
                            _to42is3000 = True
        if _toZ and _to42 and _to42is3000: pass #print 'ok'
        else: return 'Wrong ETP from B to Z'

    else:
        #retrieve etps and execute them in the proper node
        retrieve_execute_etps(exclude_ips=ignore_etp_for)
        #printmaps()
        simulate_delay()

        #print 'After node X died... check final situation'

        # Z through B towards 42.* has rtt 3100.
        gwnum,bestdev,rtt = nodelinks[node_Z, node_B]
        if node_Z.maproute.node_get(3, 42).route_getby_gw(gwnum).rem.value == 3100: pass #print 'ok'
        else: return 'Wrong final situation'

    return False

class TestCollisionDomain(unittest.TestCase):

    def setUp(self):
        pass

    def test01EtpNewChanged(self):
        '''Detecting a new link (check etp emission)'''
        msg = test_etp_new_changed(check_etp_emission=True)
        if msg: self.fail(msg)

    def test02EtpNewChanged(self):
        '''Detecting a new link (check final result)'''
        msg = test_etp_new_changed(check_etp_emission=False)
        if msg: self.fail(msg)

    def test03EtpNewDead(self):
        '''Detecting a dead link (check etp emission)'''
        msg = test_etp_new_dead(check_etp_emission=True)
        if msg: self.fail(msg)

    def test04EtpNewDead(self):
        '''Detecting a dead link (check final result)'''
        msg = test_etp_new_dead(check_etp_emission=False)
        if msg: self.fail(msg)

if __name__ == '__main__':
    unittest.main()
