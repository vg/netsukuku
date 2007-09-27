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
import ntk.sim.wrap.xtime as xtime
from ntk.lib.micro import micro, microfunc, allmicro_run

import ntk.lib.rpc as rpc
from ntk.lib.micro import micro, allmicro_run, micro_block
from ntk.network.inet import Inet

seed(1)

N=Net()
N.net_file_load('net1')
N.net_dot_dump(open('net1.dot', 'w'))


#
### The server remotable functions
#

class MyNestedMod:
    def __init__(self):
        self.remotable_funcs = [self.add]

    def add(self, x,y): return x+y

class MyMod:
    def __init__(self):
        self.nestmod = MyNestedMod()

        self.remotable_funcs = [self.square, self.mul, self.caller_test,
                                self.void_func_caller, self.void_func]

    def square(self, x): return x*x
    def mul(self, x, y): return x*y

    def private_func(self): pass

    def caller_test(self, _rpc_caller, x, y):
        c = _rpc_caller
        logging.debug("caller test: "+str([c.ip, c.port, c.dev, c.socket]))
        return (x,y)
    
    def void_func_caller(self, _rpc_caller):
        c = _rpc_caller
        logging.debug("void func caller: "+str([c.ip, c.port, c.dev, c.socket]))
    def void_func(self):
        logging.debug("void_func")

mod = MyMod()


#
#### TCP client
#
def tcp_client():
        client = rpc.TCPClient(Inet().ip_to_str(N.net[0].ip), port=PORT,
                        net=N, me=N.net[1], sockmodgen=Sock)
   
        x=5
        xsquare = client.square(x)
        print xtime.time(), 'assert xsquare == 25'
        assert xsquare == 25
        xmul7   = client.mul(x, 7)
        print xtime.time(), 'assert xmul7 == 35'
        assert xmul7 == 35
        xadd9   = client.nestmod.add(x, 9)
        print xtime.time(), 'assert xadd9 == 14'
        assert xadd9 == 14
    
        # something trickier
        n, nn = client, client.nestmod
        result = n.square(n.mul(x, nn.add(x, 10)))
    
        assert (1,2) == client.caller_test(1,2)
    
        try:
            # should crash now
            client.private_func()
        except Exception, e:
            logging.debug(str(e))

#
### Bcast client
#
def udp_client():
    client = rpc.BcastClient(Inet(), devs=['lo'], port=PORT, net=N,
                    me=N.net[1], sockmodgen=Sock)
    print xtime.time(),"calling void func"
    client.void_func()
    client.void_func_caller()
    print xtime.time(), "udp_client end"

def run_test_tcp():
    print 'Starting tcp server...'

    rpc.MicroTCPServer(mod, (Inet().ip_to_str(N.net[0].ip), PORT), 'lo', N, N.net[0], Sock)
    micro(tcp_client)
    allmicro_run()

def run_test_udp():
    print 'Starting udp server...'

    rpc.MicroUDPServer(mod, ('', PORT), 'lo', N, N.net[0], Sock)
    micro(udp_client)


if __name__ == '__main__':
  if len(sys.argv) == 1:
          print "specify udp or tcp"
          sys.exit(1)

  sim.sim_activate()

  if sys.argv[1]== 'tcp':
          run_test_tcp()
  if sys.argv[1]== 'udp':
          run_test_udp()

  sim.sim_run()
  allmicro_run()
