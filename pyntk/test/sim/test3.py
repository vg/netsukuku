# Test suite for rpc.py
import sys
import logging
from random import randint, seed

# Logging option

LOG_LEVEL = logging.DEBUG
LOG_FILE = ''

log_config = {
    'level': LOG_LEVEL,
    'format': '%(levelname)s %(message)s'
}

if LOG_FILE == '':
    log_config['stream'] = sys.stdout
else:
    log_config['filename'] = LOG_FILE

logging.basicConfig(**log_config)

sys.path.append('../../')
from ntk.sim.net import Net
import ntk.sim.sim as sim
from ntk.sim.wrap.sock import Sock
import ntk.sim.wrap.xtime as xtime
from socket import AF_INET, SOCK_DGRAM, SOCK_STREAM, SO_REUSEADDR, SOL_SOCKET
from ntk.lib.micro import micro, microfunc, allmicro_run
from ntk.sim.lib.opt import Opt

import ntk.lib.rpc as rpc
from ntk.lib.micro import micro, allmicro_run, micro_block
from ntk.ntkd import NtkNode

#Initialize the pseudo-random seed
seed(1)

# Load the network graph from net1.dot
# Its format is described in pyntk/ntk/sim/net.py inside the def
# net_file_load() function.
# An example is:
#        net = {
#
#                'A' : [ ('B', 'rtt=10, bw=6'), ('C', 'rand=1') ],
#
#                'B' : [ ('node D', 'rtt=2, bw=9') ],
#            
#                'C' : [],
#
#                'node D' : []
#        }
# The meaning is clear: you're specifying the links between nodes and their
# properties. In this example, the graph is:
#
#               A -- B -- D
#               |
#               C
#
#  Links are symmetric
#
N=Net()
N.net_file_load('net1')
N.net_dot_dump(open('net1.dot', 'w'))

# Activate the simulator
sim.sim_activate()

def run_sim():
        # Fill the options to pass to the nodes.
        o=Opt()
        o.simulated=1

        # Add the node N.net[0], and N.net[1] to the simulation. They are the
        # nodes loaded by N.net_file_load().
        micro(NtkNode(o, N, N.net[0], Sock, xtime).run)

        # Wait 10 time units before adding N.net[1]
        xtime.swait(10)
        micro(NtkNode(o, N, N.net[1], Sock, xtime).run)

#Run the simulation
sim.sim_run()
run_sim()
allmicro_run()
