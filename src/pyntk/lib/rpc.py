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
#
# Netsukuku RPC
# 
## Usage example
# 
#### The server
# 
# class MyNestedMod:
#     def __init__(self):
#         self.remotable_funcs = [self.add]
# 
#     def add(self, x,y): return x+y
# 
# class MyMod:
#     def __init__(self):
#         self.nestmod = MyNestedMod()
# 
#         self.remotable_funcs = [self.square, self.mul]
# 
#     def square(self, x): return x*x
#     def mul(self, x, y): return x*y
# 
# def another_foo():pass
# remotable_funcs = [another_foo]
# 
# mod = MyMod()
# 
# ntk_server = SimpleRPCServer()
# ntk_server.serve_forever()
# 
#
#### The client
# 
# ntk_client = SimpleRPCClient()
# x=5
# xsquare = ntk_client.mod.square(x)
# xmul7   = ntk_client.mod.mul(x, 7)
# xadd9	= ntk_client.mod.nestmod.add(x, 9)
# ntk_client.another_foo()
# 
# # something trickier
# nm = ntk_client.mod
# result = nm.square(nm.mul(x, nm.nestmod.add(x, 10)))
# 
#### Notes
# 
# - If the function on the remote side returns DoNotReply, then no
#   reply is sent.
# 
## TODO
# 
# - Add a timeout in the .recv()
#
# - Support for broadcast queries. See radar.py and how it utilises NtkdBroadcast
#
#### Done, but to be revisioned
#
# - The client must not create a new connection for each new rpc_call!
#   Create a connection just when the instance is created
#
##

import logging
import socket
import SocketServer

import rencode


DoNotReply = "__DoNotReply__"

class RPCError(Exception): pass
class RPCFuncNotRemotable(RPCError): pass
class RPCFunctionError(RPCError): pass


class FakeRmt(object):
    """A fake remote class

    This class is used to perform RPC call using the following form:

        remote_instance.mymethod1.mymethod2.func(p1, p2, p3)

    instead of:

        remote_instance.rpc_call('mymethod1.method2.func', (p1, p2, p3))
    """
    def __init__(self, name=''):
        self._name = name

    def __getattr__(self, name):
        '''
        @return: A new FakeRmt: used to accumulate instance attributes
        '''

        fr=FakeRmt(self._name + '.' + name)
	fr.rmt=self.rmt
	return fr

    def __call__(self, *params):
        return self.rmt(self._name[1:], *params)

    def rmt(self, func_name, *params):
        '''Perform the RPC call

           This method must be ovverrided in child class.
        '''
        raise NotImplementedError, 'You must override this method'


class RPCDispatcher(object):
    '''
    This class is used to register RPC function handlers and
    to dispatch them.
    '''
    def __init__(self, root_instance=None):
        self.root_instance=root_instance

    def func_get(self, func_name):
        """Returns the function (if any and if remotable), which has the given
	  `func_name'

	  `func_name' is a string in the form "mod1.mod2.mod3....modn.func"
	  or just "func". In the latter case "func" is searched in the
	  globals()
	"""
       
        logging.debug("func_get: "+str(func_name))

	def func_to_name(f): return f.__name__

	splitted = func_name.split('.')
	lens = len(splitted)
	if lens < 1:
		return None
	mods = splitted[:-1]
	func = splitted[-1] 

	if self.root_instance != None:
		p = self.root_instance
        else:
		import __main__
		p = __main__
	
	try:
		for m in mods:
			p=getattr(p, m)

		if func in map(func_to_name, p.remotable_funcs):
			return getattr(p, func)
	except AttributeError:
		return None

	return None
	
    def _dispatch(self, func_name, params):
        logging.debug("_dispatch: "+func_name+"("+str(params)+")")
        func = self.func_get(func_name)
	if func == None:
        	raise RPCFuncNotRemotable('Function %s is not remotable' % func_name)
        try:
        	return func(*params)
        except Exception, e:
        # I propagate all exceptions to `dispatch'
        	raise

    def dispatch(self, func, params):
        try:
            response = self._dispatch(func, params)
        except Exception, e:
            logging.debug(str(e))
            response = ('rmt_error', str(e))
        logging.debug("dispatch response: "+str(response))
	return response

    def marshalled_dispatch(self, sender, data):
        '''Dispatches a RPC function from marshalled data'''

	unpacked = rencode.loads(data) 
	if not isinstance(unpacked, tuple) or not len(unpacked) == 2:
		e = 'Malformed packet received from '+sender
		logging.debug(e)
                response = ('rmt_error', str(e))
        else:
		response = self.dispatch(*unpacked)

	if response != DoNotReply:
		encresp = rencode.dumps(response)
	else:
		encresp = DoNotReply
        return encresp


class NtkRequestHandler(SocketServer.BaseRequestHandler):
    '''RPC request handler class

    Handles all request and try to decode them.
    '''
    def handle(self):
        logging.debug('Connected from %s', self.client_address)

        while 1:
		try:
		    data = self.request.recv(1024)
		    logging.debug('Handling data: %s', data)
		    response = self.server.marshalled_dispatch(self.client_address, data)
		    logging.debug('Response: %s', response)
		except RPCError:
		    logging.debug('An error occurred during request handling')
		if response != DoNotReply:
		    self.request.send(response)
		    #self.request.close()
		    logging.debug('Response sended')


class SimpleRPCServer(SocketServer.TCPServer, RPCDispatcher):
    '''This class implement a simple Ntk-Rpc server'''

    def __init__(self, addr=('localhost', 269),
                 requestHandler=NtkRequestHandler,
		 root_instance=None):

	RPCDispatcher.__init__(self, root_instance)
        SocketServer.TCPServer.__init__(self, addr, requestHandler)

class SimpleRPCClient(FakeRmt):
    '''This class implement a simple Ntk-RPC client'''

    def __init__(self, host='localhost', port=269):
        self.host = host
        self.port = port

        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	self.connected = False

	FakeRmt.__init__(self)

    def rpc_call(self, func_name, params):
        '''Performs a rpc call

        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

	if not self.connected:
		self.connect()

        data = rencode.dumps((func_name, params))
        self.socket.send(data)

        recv_encoded_data = self.socket.recv(1024)
        recv_data = rencode.loads(recv_encoded_data)
	logging.debug("Recvd data: "+str(recv_data))

        # Handling errors
        # I receive a message with the following format:
        #     ('rmt_error', message_error)
        # where message_error is a string
        if isinstance(recv_data, tuple) and recv_data[0] == 'rmt_error':
            raise RPCError(recv_data[1])

        return recv_data
    
    def connect(self):
        self.socket.connect((self.host, self.port))
	self.connected = True

    def close(self):
        self.socket.close()
	self.connected = False

    def rmt(self, func_name, *params):
        return self.rpc_call(func_name, params)
    
    def __del__(self):
        self.close()
