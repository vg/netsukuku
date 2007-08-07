##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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

import logging
import socket
import SocketServer

import rencode


class NtkRPCDispatcher(object):
    '''
    This class is used to register RPC function handlers and
    to dispatch them.
    '''
    def __init__(self):
        self.funcs = {} # Dispatcher functions dictionary

    def register_function(self, func):
        '''Register a function to respond to an RPC request'''

        func_name = func.__name__
        self.funcs[func_name] = func

    def _dispatch(self, func_name, params):
        func = None
        try:
            func = self.funcs[func_name]
        except KeyError:
            raise Exception('function %s is not registered' % func_name)

        if func is not None:
            return func(*params)

    def marshalled_dispatch(self, data):
        '''Dispaches a RPC function from marshalled data'''

        func, params = rencode.loads(data) 
        
        try:
            response = self._dispatch(func, params)
        except:
            logging.debug('Dispatching function error')

        response = rencode.dumps(response)

        return response


class NtkRequestHandler(SocketServer.BaseRequestHandler):
    '''RPC request handler class
    
    Handles all request and try to decode them.
    '''
    def handle(self):
        logging.debug('Connected from %s', self.client_address)

        try:
            data = self.request.recv(1024) 
            logging.debug('Handling data: %s', data)
            response = self.server.marshalled_dispatch(data)
            logging.debug('Response: %s', response)
        except:
            pass # TODO Handle exceptions!
        else:
            self.request.send(response)
            self.request.close()
            logging.debug('Response sended')


class SimpleNtkRPCServer(SocketServer.TCPServer, NtkRPCDispatcher):
    '''Tis class implement a simple Ntk-Rpc server'''
    
    def __init__(self, addr, requestHandler=NtkRequestHandler):

        NtkRPCDispatcher.__init__(self)
        SocketServer.TCPServer.__init__(self, addr, requestHandler)

class SimpleNtkRPCClient:
    '''This class implement a simple Ntk-RPC client'''

    def __init__(self, host='localhost', port=8888):
        self.host = host
        self.port = port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))

    def rpc_call(self, func_name, params):
        '''Performs a rpc call
        
        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

        data = rencode.dumps((func_name, params))
        self.socket.send(data)
        
        recv_data = self.socket.recv(1024)
        self.socket.close()
        
        return rencode.loads(recv_data)
        

if __name__ == '__main__':
    
    import sys

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

    server = SimpleNtkRPCServer(('localhost', 8888))
    server.register_function(pow)
    server.serve_forever()

