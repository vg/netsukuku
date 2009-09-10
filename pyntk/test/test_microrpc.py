##
# This file is part of Netsukuku
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
# Test suite for rpc.py
#

import sys
sys.path.append('..')

from ntk.wrap.sock import Sock
socket=Sock()

import logging
import ntk.lib.rpc as rpc
from ntk.lib.micro import micro, allmicro_run, micro_block

from random import randint
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
        client = rpc.TCPClient(port=PORT)
   
        x=5
        xsquare = client.square(x)
        assert xsquare == 25
        xmul7   = client.mul(x, 7)
        assert xmul7 == 35
        xadd9   = client.nestmod.add(x, 9)
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
    client = rpc.BcastClient(devs=['lo'], port=PORT)
    print "calling void func"
    client.void_func()
    client.void_func_caller()
    print "udp_client end"


def run_test_tcp():
    print 'Starting tcp server...'

    rpc.MicroTCPServer(mod, ('localhost', PORT))

    micro(tcp_client)
    allmicro_run()

def run_test_udp():
    print 'Starting udp server...'

    rpc.MicroUDPServer(mod, ('', PORT))

    micro(udp_client)
    allmicro_run()


if __name__ == '__main__':
  if len(sys.argv) == 1:
          print "specify udp or tcp"
          sys.exit(1)

  if sys.argv[1]== 'tcp':
          run_test_tcp()
  if sys.argv[1]== 'udp':
          run_test_udp()
