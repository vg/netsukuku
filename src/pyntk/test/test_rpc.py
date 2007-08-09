# Test suite for rpc.py

import sys
sys.path.append('..')

import logging
import threading

from lib.rpc import SimpleNtkRPCServer, SimpleNtkRPCClient

REQUEST = 3
PORT = 8888

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

class MyClass:

    def my_cool_method(self, x):
        return x * 2

    def my_cool_method2(self, x):
        return x[1] * 2

class MixinServer:
    def serve_few(self):
        for _ in range(REQUEST):
            self.handle_request()

class NtkRPCFewServer(MixinServer, SimpleNtkRPCServer):
    '''A SimpleNtkRPCServer that handles only `REQUEST' requests'''

class ThreadedNtkRPCServer(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        ntk_server = NtkRPCFewServer(('localhost', PORT))
        ntk_server.register_instance(MyClass())
        ntk_server.serve_few()

class ThreadedNtkRPCClient(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        ntk_client = SimpleNtkRPCClient(port=PORT)

        response = ntk_client.rpc_call('my_cool_method', (3,))
        assert response == 6
        # Use try to catch RPCError if you don't want to crash :D
        try:
            response = ntk_client.rpc_call('my_cool_method2', ())
        except: pass
        response = ntk_client.rpc_call('my_cool_method3', ()) # not exist!

if __name__ == '__main__':
    print 'Starting server...'
    server = ThreadedNtkRPCServer()
    server.start()

    client = ThreadedNtkRPCClient()
    client.start()

    client.join()
    server.join()
