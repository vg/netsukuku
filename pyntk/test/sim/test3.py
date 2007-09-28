# Test suite for rpc.py
import sys
import logging
REQUEST = 3
from random import randint, seed
PORT =8888
PORT=randint(8880, 8889)

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
from ntk.lib.opt import Opt

import ntk.lib.rpc as rpc
from ntk.lib.micro import micro, allmicro_run, micro_block
from ntk.network.inet import Inet
from ntk.ntkd import NtkNode

seed(1)

N=Net()
N.net_file_load('net1')
N.net_dot_dump(open('net1.dot', 'w'))

sim.sim_activate()

def run_sim():
        o=Opt()
        o.simulated=1
        micro(NtkNode(o, N, N.net[0], Sock, xtime).run)
        xtime.swait(10)
        micro(NtkNode(o, N, N.net[1], Sock, xtime).run)

sim.sim_run()
run_sim()
allmicro_run()
