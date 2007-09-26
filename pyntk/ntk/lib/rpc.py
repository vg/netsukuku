##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
"""
# Usage example

import rpc

### The server

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

 mod = MyMod()

 server = rpc.TCPServer(mod)
 server.serve_forever()


### The client

 client = rpc.TCPClient()
 x=5
 xsquare = client.square(x)
 xmul7   = client.mul(x, 7)
 xadd9 = client.nestmod.add(x, 9)

 # something trickier
 n, nn = client, client.nestmod
 result = n.square(n.mul(x, nn.add(x, 10)))
"""

## TODO
#
# - Describe the _rpc_caller feature
#
#### Done, but to be revisioned
#
# - The client must not create a new connection for each new rpc_call!
#   Create a connection just when the instance is created
#
##

import struct
import logging
import socket
import sys
try: del sys.modules['SocketServer']
except: pass 
import SocketServer as SckSrv

import ntk.lib.rencode as rencode
from   ntk.lib.micro import  micro, microfunc

class RPCError(Exception): pass
class RPCNetError(Exception): pass
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

class CallerInfo(object):
    __slots__ = ['ip','port','dev','socket']
    def __init__(self, ip=None, port=None, dev=None, socket=None):
        self.ip=ip
        self.port=port
        self.dev=dev
        self.socket=socket

class RPCDispatcher(object):
    '''
    This class is used to register RPC function handlers and
    to dispatch them.
    '''
    def __init__(self, root_instance):
        self.root_instance=root_instance

    def func_get(self, func_name):
        """Returns the function (if any and if remotable), which has the given
          `func_name'

          `func_name' is a string in the form "mod1.mod2.mod3....modn.func"
          or just "func". In the latter case "func" is searched in the
          globals()
        """

        logging.debug("func_get: "+str(func_name))

        splitted = func_name.split('.')

        if not len(splitted):
            return None

        mods, func = splitted[:-1], splitted[-1]

        p = self.root_instance
        try:
            for m in mods:
                p = getattr(p, m)

            if func in map(lambda f:f.__name__, p.remotable_funcs):
                return getattr(p, func)
        except AttributeError:
            return None

        return None

    def _dispatch(self, caller, func_name, params):
        logging.debug("_dispatch: "+func_name+"("+str(params)+")")
        func = self.func_get(func_name)
        if func == None:
            raise RPCFuncNotRemotable('Function %s is not remotable' % func_name)
        try:
            if '_rpc_caller' in func.im_func.func_code.co_varnames:
                return func(caller, *params)
            else:
                return func(*params)
        except Exception, e:
        # I propagate all exceptions to `dispatch'
            raise

    def dispatch(self, caller, func, params):
        try:
            response = self._dispatch(caller, func, params)
        except Exception, e:
            logging.debug(str(e))
            response = ('rmt_error', str(e))
        logging.debug("dispatch response: "+str(response))
        return response

    def marshalled_dispatch(self, caller, data):
        '''Dispatches a RPC function from marshalled data'''

        unpacked = rencode.loads(data)
        if not isinstance(unpacked, tuple) or not len(unpacked) == 2:
            e = 'Malformed packet received from '+caller.ip
            logging.debug(e)
            response = ('rmt_error', str(e))
        else:
            response = self.dispatch(caller, *unpacked)

        return rencode.dumps(response)

### Code taken from examples/networking/rpc.py of stackless python
#
#
_data_hdr_sz = struct.calcsize("I")
def _data_pack(data):
    return struct.pack("I", len(data)) + data

def _data_unpack_from_stream_socket(socket):
    readBuffer = ""
    while True:
        rawPacket = socket.recv(_data_hdr_sz-len(readBuffer))
        if not rawPacket:
            return ""
        readBuffer += rawPacket
        if len(readBuffer) == _data_hdr_sz:
            dataLength = struct.unpack("I", readBuffer)[0]
            readBuffer = ""
            while len(readBuffer) != dataLength:
                rawPacket = socket.recv(dataLength - len(readBuffer))
                if not rawPacket:
                    return ""
                readBuffer += rawPacket
            return readBuffer
#
###

def _data_unpack_from_buffer(buffer):
    readBuffer = ""
    buflen = len(buffer)
    if buflen < _data_hdr_sz:
        return ""

    dataLength = struct.unpack("I", buffer[:_data_hdr_sz])[0]
    if buflen == dataLength+_data_hdr_sz:
        return buffer[_data_hdr_sz:]

    return ""


class MicroMixin:
    @microfunc(True)
    def process_request_micro(self, request, client_address):
        self.finish_request(request, client_address)
        self.close_request(request)
        
    def process_request(self, request, client_address):
        self.process_request_micro(request, client_address)

class StreamRequestHandler(SckSrv.BaseRequestHandler):
    '''RPC stream request handler class

    Handles all request and try to decode them.
    '''
    def setup(self):
        logging.debug('Connected from %s, dev %s', self.client_address, self.server.dev)
        self.caller = CallerInfo(self.client_address[0], self.client_address[1],
                                self.server.dev, self.request)
    def handle(self):
        while True:
            try:
                data = _data_unpack_from_stream_socket(self.request)
                if not data: break
                logging.debug('Handling data: %s', data)
                response = self.server.marshalled_dispatch(self.caller, data)
                logging.debug('Response: %s', response)
            except RPCError:
                logging.debug('An error occurred during request handling')

            self.request.send(_data_pack(response))
            #self.request.close()
            logging.debug('Response sent')


class TCPServer(SckSrv.TCPServer, RPCDispatcher):
    '''This class implement a simple Rpc server'''

    def __init__(self, root_instance, addr=('', 269), dev=None,
                    requestHandler=StreamRequestHandler):

        self.dev=dev
        #TODO: if dev!=None: bind to device the listening socket
        RPCDispatcher.__init__(self, root_instance)
        SckSrv.TCPServer.__init__(self, addr, requestHandler)
        self.allow_reuse_address=True

class MicroTCPServer(MicroMixin, TCPServer): pass

class TCPClient(FakeRmt):
    '''This class implement a simple TCP RPC client'''

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
        self.socket.sendall(_data_pack(data))

        recv_encoded_data = _data_unpack_from_stream_socket(self.socket)
        if not recv_encoded_data:
                raise RPCNetError, 'connection closed before reply'
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


class DgramRequestHandler(SckSrv.BaseRequestHandler):
    '''RPC stream request handler class

    Handles all request and try to decode them.
    '''
    def setup(self):
        self.packet, self.socket = self.request
        self.caller = CallerInfo(self.client_address[0], self.client_address[1],
                                self.server.dev, self.socket)
        logging.debug('UDP packet from %s, dev %s', self.client_address, self.server.dev)

    def handle(self):

        try:
            data = _data_unpack_from_buffer(self.packet)
            logging.debug('Handling data: %s', data)
            response = self.server.marshalled_dispatch(self.caller, data)
        except RPCError:
            logging.debug('An error occurred during request handling')

class UDPServer(SckSrv.UDPServer, RPCDispatcher):
    '''This class implement a simple Rpc UDP server

    *WARNING*
    If the message to be received is greater than the buffer
    size, it will be lost! (buffer size is 8Kb)
    Use UDP RPC only for small calls, i.e. if the arguments passed to the
    remote function are small when packed.
    *WARNING*
    '''

    def __init__(self, root_instance, addr=('', 269), dev=None,
                    requestHandler=DgramRequestHandler):
        self.dev=dev
        #TODO: if dev!=None: bind to device the listening socket
        RPCDispatcher.__init__(self, root_instance)
        SckSrv.UDPServer.__init__(self, addr, requestHandler)
        self.allow_reuse_address=True

class MicroUDPServer(MicroMixin, UDPServer): pass

class BcastClient(FakeRmt):
    '''This class implement a simple Broadcast RPC client

    *WARNING*
    If the message to be received by the remote side is greater than the buffer
    size, it will be lost! (buffer size is 8Kb)
    Use UDP RPC only for small calls, i.e. if the arguments passed to the
    remote function are small when packed.
    *WARNING*
    '''

    def __init__(self, inet, devs=[], port=269):
        """
        inet:  network.inet.Inet instance
        devs:  list of devices where to send the broadcast calls
        If devs=[], the msg calls will be sent through all the available
        devices"""

        self.port = port
        self.inet = inet

        self.dev_sk = {}
        for d in devs:
            self.dev_sk[d] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
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
        self.send(_data_pack(data))

    def send(self, data):
        for d, sk in self.dev_sk.iteritems():
            sk.sendto(data, ('<broadcast>', self.port))

    def connect(self):
        for d, sk in self.dev_sk.iteritems():
            #self.inet.sk_bindtodevice(sk, d)
            self.inet.sk_set_broadcast(sk, d)
            sk.connect(('<broadcast>', self.port))
        self.connected = True

    def close(self):
        for d, sk in self.dev_sk.iteritems():
            sk.close()
        self.connected = False

    def rmt(self, func_name, *params):
        self.rpc_call(func_name, params)

    def __del__(self):
        self.close()
