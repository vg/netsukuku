# Test suite for rpc.py

import sys
sys.path.append('..')

import logging
import threading

from lib.rpc import SimpleRPCServer, SimpleRPCClient

REQUEST = 3
from random import randint
PORT = randint(8880, 8889)

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
###The server
#

class MyNestedMod:
    def __init__(self):
        self.remotable_funcs = [self.add]

    def add(self, x,y): return x+y

class MyMod:
    def __init__(self):
        self.nestmod = MyNestedMod()

        self.remotable_funcs = [self.square, self.mul]

    def square(self, x): return x*x
    def mul(self, x, y): return x*y

    def private_func(self): pass

mod = MyMod()

class MixinServer:
    def serve_few(self):
        for _ in range(REQUEST):
            self.handle_request()

class RPCFewServer(MixinServer, SimpleRPCServer):
    '''A SimpleRPCServer that handles only `REQUEST' requests'''

class ThreadedRPCServer(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        server = RPCFewServer(mod, ('localhost', PORT))
        server.serve_few()

#
#### The client
#

class ThreadedRPCClient(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        client = SimpleRPCClient(port=PORT)

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

        try:
            # should crash now
            client.private_func()
        except Exception, e:
            logging.debug(str(e))


if __name__ == '__main__':
    print 'Starting server...'

    server = ThreadedRPCServer()
    server.start()

    client = ThreadedRPCClient()
    client.start()

    client.join()
    server.join()
