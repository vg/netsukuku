# Test suite for rpc.py

import sys
sys.path.append('..')

import logging
import threading

import lib.rpc as rpc
from network.inet import Inet

REQUEST = 3
from random import randint
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
### TCP server
#

class MixinServer:
    def serve_few(self):
        for _ in range(REQUEST):
            self.handle_request()

class RPCFewServer(MixinServer, rpc.TCPServer):
    '''A rpc.TCPServer that handles only `REQUEST' requests'''

class ThreadedRPCServer(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        server = RPCFewServer(mod, ('localhost', PORT))
        server.serve_few()


#
#### TCP client
#

class ThreadedRPCClient(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
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
### UDP server
#
class UDPFewServer(MixinServer, rpc.UDPServer):
    '''A rpc.UDPServer that handles only `REQUEST' requests'''

class ThreadedUDPServer(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        server = UDPFewServer(mod, ('', PORT))
        server.serve_few()

#
### Bcast client
#

class ThreadedBcastRPCClient(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
    def run(self):
        client = rpc.BcastClient(Inet(), devs=['lo'], port=PORT)

        client.void_func()
        client.void_func_caller()
    
if __name__ == '__main__':
  if len(sys.argv) == 1:
	  print "specify udp or tcp"
	  sys.exit(1)

  if sys.argv[1]== 'tcp':
    print 'Starting tcp server...'

    server = ThreadedRPCServer()
    server.start()

    client = ThreadedRPCClient()
    client.start()

    client.join()
    server.join()


  if sys.argv[1]== 'udp':
    print 'Starting udp server...'

    server = ThreadedUDPServer()
    server.start()

    client = ThreadedBcastRPCClient()
    client.start()

    client.join()
    server.join()
