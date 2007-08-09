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
'''Netsukuku RPC

Usage example:

- Register functions

ntk_server = SimpleNtkRPCServer()
ntk_server.register_function(my_cool_function)
ntk_server.serve_forever()

- Register an instance

class MyClass:

    def my_cool_method(self, x):
        return x*2

ntk_server = SimpleNtkRPCServer()
ntk_server.register_instance(MyClass())
ntk_server.serve_forever()

'''
import logging
import socket
import SocketServer

import rencode


class RPCError(Exception):
    pass

class RPCFunctionNotRegistered(RPCError):
    pass

class RPCFunctionError(RPCError):
    pass


class FakeNtkd(object):
    '''A fake Ntkd class

    This class is used to perform RPC call using the following form:

        ntkd.ept.mymethod(p1, p2, p3)

    instead of:

        ntkd.rmt('etp.mymethod', (p1, p2, p3))
    '''
    def __init__(self, name=''):
        self._name = name

    def __getattr__(self, name):
        '''
        @return: A new FakeNtkd: used to accumulate instance attributes
        '''

        return FakeNtkd(self._name + '.' + name)

    def __call__(self, *params):
        self.rmt(self._name[1:], params)

    def rmt(self, func_name, *params):
        '''Perform the RPC call

           This method must be ovverrided in child class.
        '''
        raise NotImplementedError, 'You must override this method'



class NtkRPCDispatcher(object):
    '''
    This class is used to register RPC function handlers and
    to dispatch them.
    '''
    def __init__(self):
        self.funcs = {} # Dispatcher functions dictionary
        self.instance = None

    def register_function(self, func):
        '''Register a function to respond to an RPC request'''

        func_name = func.__name__
        self.funcs[func_name] = func

    def register_instance(self, instance):
        '''Register an instance'''
        self.instance = instance

    def _dispatch(self, func_name, params):
        func = None
        try:
            func = self.funcs[func_name]
        except KeyError:
            if self.instance is not None:
                try:
                    if not func_name.startswith('_'):
                        func = getattr(self.instance, func_name)
                except AttributeError:
                    pass

        if func is not None:
            try:
                return func(*params)
            # I propagate all exceptions to `marshalled_dispatch'
            except Exception, e:
                raise
        else:
            raise RPCFunctionNotRegistered(
                        'Function %s is not registered' % func_name)

    def marshalled_dispatch(self, data):
        '''Dispaches a RPC function from marshalled data'''

        func, params = rencode.loads(data) 

        try:
            response = self._dispatch(func, params)
        except Exception, e:
            logging.debug(str(e))
            response = ('rmt_error', str(e))
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
        except RPCError:
            logging.debug('An error occurred during request handling')
        else:
            self.request.send(response)
            self.request.close()
            logging.debug('Response sended')


class SimpleNtkRPCServer(SocketServer.TCPServer, NtkRPCDispatcher):
    '''This class implement a simple Ntk-Rpc server'''

    def __init__(self, addr=('localhost', 269),
                 requestHandler=NtkRequestHandler):

        NtkRPCDispatcher.__init__(self)
        SocketServer.TCPServer.__init__(self, addr, requestHandler)

class SimpleNtkRPCClient:
    '''This class implement a simple Ntk-RPC client'''

    def __init__(self, host='localhost', port=269):
        self.host = host
        self.port = port

    def rpc_call(self, func_name, params):
        '''Performs a rpc call

        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))

        data = rencode.dumps((func_name, params))
        self.socket.send(data)

        recv_encoded_data = self.socket.recv(1024)
        self.socket.close()

        recv_data = rencode.loads(recv_encoded_data)

        # Handling errors
        # I receive a message with the following format:
        #     ('rmt_error', message_error)
        # where message_error is a string
        if isinstance(recv_data, tuple) and recv_data[0] == 'rmt_error':
            raise RPCError(recv_data[1])

        return recv_data
