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

import ntk.wrap.xtime as xtime
from ntk.lib.micro import microfunc, micro, micro_block, allmicro_run
from ntk.network.inet import ip_to_str
from ntk.config import settings

from etp_simulator import create_node, retrieve_execute_etps
from etp_simulator import simulated_nodes, remove_simulated_node_by_ip
from etp_simulator import set_functions

#######################################
##  Remember to add a little delay  (eg simulate_delay())
##  after each simulated signal or event.
##  This will provide enough micro_block to completely
##  execute each started microthread.
def simulate_delay():
    xtime.swait(400)

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

def node_gonna_exec_etp(node):
    pass #log_executing_node(node)
def delay_each_etp_exec():
    return 400
def nic_set_address(the_NIC, address):
    pass #print 'setting address ' + str(address)
def nic_init(the_NIC, nic):
    pass #print 'nic ' + nic
def route_add(ip, cidr, dev=None, gateway=None):
    pass #print 'add route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def route_change(ip, cidr, dev=None, gateway=None):
    pass #print 'change route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def route_delete(ip, cidr, dev=None, gateway=None):
    pass #print 'delete route (ip, cidr, dev, gateway) ' + str((ip, cidr, dev, gateway))
def exec_rpc_call(ip, pieces, params):
    pass #print 'To ip ' + ip_to_str(ip) + ': ' + '.'.join(pieces) + str(params)

set_functions(_node_gonna_exec_etp=node_gonna_exec_etp,
              _nic_set_address=nic_set_address,
              _nic_init=nic_init,
              _route_add=route_add,
              _route_change=route_change,
              _route_delete=route_delete,
              _delay_each_etp_exec=delay_each_etp_exec,
              _exec_rpc_call=exec_rpc_call)

#########################################################
#   |--eth1   eth0--|                                   #
#   |       A       |--eth0   eth1--|--<restodintk>     #
#   |               |       B                           #
#   |                                                   #
#   |--eth0                                             #
#   |       C                                           #
#   |                                                   #
#   |--eth0   eth1--|--<restodintk>                     #
#   |       D                                           #
#   |                                                   #
#   |--eth0   eth1--<void>                              #
#   |       E                                           #
#########################################################

node_A = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1'], netid=12345)
node_B = create_node(firstnip=[2,2,2,2], nics=['eth0', 'eth1'], netid=12345)
node_C = create_node(firstnip=[3,3,3,3], nics=['eth0'], netid=12345)
node_D = create_node(firstnip=[4,4,4,4], nics=['eth0', 'eth1'], netid=12345)
node_E = create_node(firstnip=[5,5,5,5], nics=['eth0', 'eth1'], netid=12345)
simulate_delay()

# node_B is a new neighbour (the first) to node_A
log_executing_node(node_A)
node_A.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              1,
	              node_A.maproute.nip_to_ip(node_B.maproute.me),
	              node_B.neighbour.netid)
simulate_delay()

# node_A is a new neighbour (the first) to node_B
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              1,
	              node_B.maproute.nip_to_ip(node_A.maproute.me),
	              node_A.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()



# node_C is a new neighbour (the second) to node_A
log_executing_node(node_A)
node_A.neighbour.send_event_neigh_new(
	              ('eth1',100),
	              {'eth1':100},
	              2,
	              node_A.maproute.nip_to_ip(node_C.maproute.me),
	              node_C.neighbour.netid)
simulate_delay()

# node_A is a new neighbour (the first) to node_C
log_executing_node(node_C)
node_C.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              1,
	              node_C.maproute.nip_to_ip(node_A.maproute.me),
	              node_A.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()

#########################################################
#   |--eth1   eth0--|                                   #
#   |       A       |--eth0   eth1--|--<restodintk>     #
#   |               |       B                           #
#   |                                                   #
#   |--eth0                                             #
#   |       C                                           #
#   |                                                   #
#   |--eth0   eth1--|--<restodintk>                     #
#   |       D                                           #
#   |                                                   #
#   |--eth0   eth1--<void>                              #
#   |       E                                           #
#########################################################


# node_D is a new neighbour (the third) to node_A
log_executing_node(node_A)
node_A.neighbour.send_event_neigh_new(
	              ('eth1',100),
	              {'eth1':100},
	              3,
	              node_A.maproute.nip_to_ip(node_D.maproute.me),
	              node_D.neighbour.netid)
simulate_delay()

# node_D is a new neighbour (the second) to node_C
log_executing_node(node_C)
node_C.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              2,
	              node_C.maproute.nip_to_ip(node_D.maproute.me),
	              node_D.neighbour.netid)
simulate_delay()

# node_A is a new neighbour (the first) to node_D
log_executing_node(node_D)
node_D.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              1,
	              node_D.maproute.nip_to_ip(node_A.maproute.me),
	              node_A.neighbour.netid)
simulate_delay()

# node_C is a new neighbour (the second) to node_D
log_executing_node(node_D)
node_D.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              2,
	              node_D.maproute.nip_to_ip(node_C.maproute.me),
	              node_C.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


#########################################################
#   |--eth1   eth0--|                                   #
#   |       A       |--eth0   eth1--|--<restodintk>     #
#   |               |       B                           #
#   |                                                   #
#   |--eth0                                             #
#   |       C                                           #
#   |                                                   #
#   |--eth0   eth1--|--<restodintk>                     #
#   |       D                                           #
#   |                                                   #
#   |--eth0   eth1--<void>                              #
#   |       E                                           #
#########################################################


# node_E is a new neighbour (the fourth) to node_A
log_executing_node(node_A)
node_A.neighbour.send_event_neigh_new(
	              ('eth1',100),
	              {'eth1':100},
	              4,
	              node_A.maproute.nip_to_ip(node_E.maproute.me),
	              node_E.neighbour.netid)
simulate_delay()

# node_E is a new neighbour (the third) to node_C
log_executing_node(node_C)
node_C.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              3,
	              node_C.maproute.nip_to_ip(node_E.maproute.me),
	              node_E.neighbour.netid)
simulate_delay()

# node_E is a new neighbour (the second) to node_D
log_executing_node(node_D)
node_D.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              2,
	              node_D.maproute.nip_to_ip(node_E.maproute.me),
	              node_E.neighbour.netid)
simulate_delay()

# node_A is a new neighbour (the first) to node_E
log_executing_node(node_E)
node_E.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              1,
	              node_E.maproute.nip_to_ip(node_A.maproute.me),
	              node_A.neighbour.netid)
simulate_delay()

# node_C is a new neighbour (the second) to node_E
log_executing_node(node_E)
node_E.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              2,
	              node_E.maproute.nip_to_ip(node_C.maproute.me),
	              node_C.neighbour.netid)
simulate_delay()

# node_D is a new neighbour (the third) to node_E
log_executing_node(node_E)
node_E.neighbour.send_event_neigh_new(
	              ('eth0',100),
	              {'eth0':100},
	              3,
	              node_E.maproute.nip_to_ip(node_D.maproute.me),
	              node_D.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()

# clear all
remove_simulated_node_by_ip(node_A.maproute.me)
remove_simulated_node_by_ip(node_B.maproute.me)
remove_simulated_node_by_ip(node_C.maproute.me)
remove_simulated_node_by_ip(node_D.maproute.me)
remove_simulated_node_by_ip(node_E.maproute.me)

#################################################
#                  |               |            #
#< >--eth1   eth0--|               |--eth0      #
#          X       |               |       Y    #
#                  |--eth0   eth1--|            #
#                  |       B       |            #
#                  |               |            #
#            eth0--|               |--eth0      #
#          W       |               |       Z    #
#################################################

node_X = create_node(firstnip=[1,1,1,1], nics=['eth0', 'eth1'], netid=12345)
node_W = create_node(firstnip=[2,2,2,2], nics=['eth0'], netid=12345)
node_B = create_node(firstnip=[3,3,3,3], nics=['eth0', 'eth1'], netid=12345)
node_Y = create_node(firstnip=[4,4,4,4], nics=['eth0'], netid=12345)
node_Z = create_node(firstnip=[5,5,5,5], nics=['eth0'], netid=12345)
simulate_delay()

# X(eth0)(n1) - B(eth0)(n1)
log_executing_node(node_X)
node_X.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_X.maproute.nip_to_ip(node_B.maproute.me),
                      node_B.neighbour.netid)
simulate_delay()
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_B.maproute.nip_to_ip(node_X.maproute.me),
                      node_X.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()
# W(eth0)(n1) - B(eth0)(n2)
log_executing_node(node_W)
node_W.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_W.maproute.nip_to_ip(node_B.maproute.me),
                      node_B.neighbour.netid)
simulate_delay()
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_B.maproute.nip_to_ip(node_W.maproute.me),
                      node_W.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()
# W(eth0)(n2) - X(eth0)(n2)
log_executing_node(node_W)
node_W.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_W.maproute.nip_to_ip(node_X.maproute.me),
                      node_X.neighbour.netid)
simulate_delay()
log_executing_node(node_X)
node_X.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_X.maproute.nip_to_ip(node_W.maproute.me),
                      node_W.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()
# Y(eth0)(n1) - B(eth1)(n3)
log_executing_node(node_Y)
node_Y.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_Y.maproute.nip_to_ip(node_B.maproute.me),
                      node_B.neighbour.netid)
simulate_delay()
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      3,
                      node_B.maproute.nip_to_ip(node_Y.maproute.me),
                      node_Y.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()
# Z(eth0)(n1) - B(eth1)(n4)
log_executing_node(node_Z)
node_Z.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_Z.maproute.nip_to_ip(node_B.maproute.me),
                      node_B.neighbour.netid)
simulate_delay()
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      4,
                      node_B.maproute.nip_to_ip(node_Z.maproute.me),
                      node_Z.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()
# Z(eth0)(n2) - Y(eth0)(n2)
log_executing_node(node_Z)
node_Z.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_Z.maproute.nip_to_ip(node_Y.maproute.me),
                      node_Y.neighbour.netid)
simulate_delay()
log_executing_node(node_Y)
node_Y.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_Y.maproute.nip_to_ip(node_Z.maproute.me),
                      node_Z.neighbour.netid)
simulate_delay()
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()

## node_X dies
node_X_nip = node_X.maproute.me
node_X_ip = node_X.maproute.nip_to_ip(node_X_nip)
node_X_netid = node_X.neighbour.netid
remove_simulated_node_by_ip(node_X_ip)
## node_B detects it
log_executing_node(node_B)
node_B.neighbour.send_event_neigh_deleted(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_X_ip,
                      node_X_netid)
simulate_delay()
## node_guest3 detects it too
log_executing_node(node_W)
node_W.neighbour.send_event_neigh_deleted(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_X_ip,
                      node_X_netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


class TestLucaRoute(unittest.TestCase):

    def setUp(self):
        pass

    def testFake(self):
        ''' Fake test'''
        pass

if __name__ == '__main__':
    unittest.main()
