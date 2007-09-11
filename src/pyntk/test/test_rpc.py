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

def another_foo():pass

remotable_funcs = [another_foo]
mod = MyMod()

class MixinServer:
    def serve_few(self):
        for _ in range(REQUEST):
            self.handle_request()

class NtkRPCFewServer(MixinServer, SimpleRPCServer):
    '''A SimpleRPCServer that handles only `REQUEST' requests'''

class ThreadedNtkRPCServer(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        ntk_server = NtkRPCFewServer(('localhost', PORT))
        ntk_server.serve_few()

#
#### The client
#

class ThreadedNtkRPCClient(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        ntk_client = SimpleRPCClient(port=PORT)

        x=5
        xsquare = ntk_client.mod.square(x)
        assert xsquare == 25
        xmul7   = ntk_client.mod.mul(x, 7)
        assert xmul7 == 35
        xadd9   = ntk_client.mod.nestmod.add(x, 9)
        assert xadd9 == 14
        ntk_client.another_foo()

        # something trickier
        nm = ntk_client.mod
        result = nm.square(nm.mul(x, nm.nestmod.add(x, 10)))

        try:
            # should crash now
            ntk_client.private_func()
        except Exception, e:
            logging.debug(str(e))


if __name__ == '__main__':
    print 'Starting server...'

    server = ThreadedNtkRPCServer()
    server.start()

    client = ThreadedNtkRPCClient()
    client.start()

    client.join()
    server.join()
