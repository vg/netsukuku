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

###############################################################
#                               A                             #
#           eth0 ---------------------------- eth0            #
#      guest1                                     guest3      #
#     eth1                                           eth1     #
#      |                                              |       #
#      |                                              |       #
#  B   |                                              |   D   #
#      |                                              |       #
#     eth1                                          eth1      #
#      guest2                                     guest4      #
#           eth0 ---------------------------- eth0            #
#                               C                             #
###############################################################


node_guest1 = create_node(firstnip=[4,3,2,1], nics=['eth0', 'eth1'], netid=12345)
node_guest2 = create_node(firstnip=[8,7,6,5], nics=['eth0', 'eth1'], netid=12345)
simulate_delay()

# node_guest2 is a new neighbour (the first) to node_guest1
log_executing_node(node_guest1)
node_guest1.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      1,
                      node_guest1.maproute.nip_to_ip(node_guest2.maproute.me),
                      node_guest2.neighbour.netid)
simulate_delay()

# node_guest1 is a new neighbour (the first) to node_guest2
log_executing_node(node_guest2)
node_guest2.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      1,
                      node_guest2.maproute.nip_to_ip(node_guest1.maproute.me),
                      node_guest1.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


node_guest3 = create_node(firstnip=[12,11,10,9], nics=['eth0', 'eth1'], netid=12345)
simulate_delay()

# node_guest3 is a new neighbour (the second) to node_guest1
log_executing_node(node_guest1)
node_guest1.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_guest1.maproute.nip_to_ip(node_guest3.maproute.me),
                      node_guest3.neighbour.netid)
simulate_delay()

# node_guest1 is a new neighbour (the first) to node_guest3
log_executing_node(node_guest3)
node_guest3.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      1,
                      node_guest3.maproute.nip_to_ip(node_guest1.maproute.me),
                      node_guest1.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


node_guest4 = create_node(firstnip=[16,15,14,13], nics=['eth0', 'eth1'], netid=12345)
simulate_delay()

# node_guest4 is a new neighbour (the second) to node_guest3
log_executing_node(node_guest3)
node_guest3.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      2,
                      node_guest3.maproute.nip_to_ip(node_guest4.maproute.me),
                      node_guest4.neighbour.netid)
simulate_delay()

# node_guest3 is a new neighbour (the first) to node_guest4
log_executing_node(node_guest4)
node_guest4.neighbour.send_event_neigh_new(
                      ('eth1',100),
                      {'eth1':100},
                      1,
                      node_guest4.maproute.nip_to_ip(node_guest3.maproute.me),
                      node_guest3.neighbour.netid)
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


# A new link between guest2 and guest4 is now active.
# node_guest2 is a new neighbour (the second) to node_guest4
log_executing_node(node_guest4)
node_guest4.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_guest4.maproute.nip_to_ip(node_guest2.maproute.me),
                      node_guest2.neighbour.netid)
simulate_delay()

# node_guest4 is a new neighbour (the second) to node_guest2
log_executing_node(node_guest2)
node_guest2.neighbour.send_event_neigh_new(
                      ('eth0',100),
                      {'eth0':100},
                      2,
                      node_guest2.maproute.nip_to_ip(node_guest4.maproute.me),
                      node_guest4.neighbour.netid)
simulate_delay()


#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()


## node_guest1 dies
node_guest1_nip = node_guest1.maproute.me
node_guest1_ip = node_guest1.maproute.nip_to_ip(node_guest1_nip)
node_guest1_netid = node_guest1.neighbour.netid
remove_simulated_node_by_ip(node_guest1_ip)
## node_guest2 detects it
log_executing_node(node_guest2)
node_guest2.neighbour.send_event_neigh_deleted(
                      1,
                      node_guest1_ip,
                      node_guest1_netid,
                      ('eth1',100),
                      {'eth1':100})
simulate_delay()
## node_guest3 detects it too
log_executing_node(node_guest3)
node_guest3.neighbour.send_event_neigh_deleted(
                      1,
                      node_guest1_ip,
                      node_guest1_netid,
                      ('eth0',100),
                      {'eth0':100})
simulate_delay()

#retrieve etps and execute them in the proper node
retrieve_execute_etps()
for node in simulated_nodes:
    log_executing_node(node)
    print node.maproute.repr_me()
simulate_delay()

class TestSynchRoute(unittest.TestCase):

    def setUp(self):
        pass

    def testFake(self):
        ''' Fake test'''
        pass

if __name__ == '__main__':
    unittest.main()
