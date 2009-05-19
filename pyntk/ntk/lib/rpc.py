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

 rpc.TCPServer(mod)

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
import sys

import ntk.lib.rencode as rencode
import ntk.wrap.xtime as xtime

from ntk.lib.log import logger as logging
from ntk.lib.micro import  micro, microfunc
from ntk.network.inet import sk_set_broadcast, sk_bindtodevice
from ntk.wrap.sock import Sock



class RPCError(Exception): pass
class RPCNetError(RPCError): pass
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

        fr = FakeRmt(self._name + '.' + name)
        fr.rmt = self.rmt
        return fr

    def __call__(self, *params):
        return self.rmt(self._name[1:], *params)

    def rmt(self, func_name, *params):
        '''Perform the RPC call

           This method must be ovverrided in child class.
        '''
        raise NotImplementedError('You must override this method')

class CallerInfo(object):
    __slots__ = ['ip','port','dev','socket']

    def __init__(self, ip=None, port=None, dev=None, socket=None):
        self.ip=ip
        self.port=port
        self.dev=dev
        self.socket=socket

    def __str__(self):
        return '%s %s' % (self.ip, self.port)

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

        if not 'radar' in func_name:
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
        if not 'radar' in func_name:
            logging.debug("_dispatch: "+func_name+"("+str(params)+")")
        func = self.func_get(func_name)
        if func is None:
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
        if not 'radar' in func:
            logging.debug("dispatch response: "+str(response))
        return response

    def marshalled_dispatch(self, caller, data):
        '''Dispatches a RPC function from marshalled data'''
        error=0
        try:
                unpacked = rencode.loads(data)
        except ValueError:
                error=1
        if error or not isinstance(unpacked, tuple) or not len(unpacked) == 2:
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

def stream_request_handler(sock, clientaddr, dev, rpcdispatcher):
    logging.debug('Connected from %s, dev %s', clientaddr, dev)
    caller = CallerInfo(clientaddr[0], clientaddr[1], dev, sock)
    while True:
        try:
            data = _data_unpack_from_stream_socket(sock)
            if not data: break
            logging.debug('Handling data: %s', data)
            response = rpcdispatcher.marshalled_dispatch(caller, data)
            logging.debug('Response: %s', response)
        except RPCError:
            logging.debug('An error occurred during request handling')

        sock.send(_data_pack(response))
        #self.request.close()
        logging.debug('Response sent')
    sock.close()

def micro_stream_request_handler(sock, clientaddr, dev, rpcdispatcher):
    micro(stream_request_handler, (sock, clientaddr, dev, rpcdispatcher))

def TCPServer(root_instance, addr=('', 269), dev=None, net=None, me=None,
                sockmodgen=Sock, request_handler=stream_request_handler):
    socket=sockmodgen(net, me)
    s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(8)
    rpcdispatcher=RPCDispatcher(root_instance)
    while 1: 
        sock, clientaddr = s.accept()
        request_handler(sock, clientaddr, dev, rpcdispatcher)

@microfunc(True)
def MicroTCPServer(root_instance, addr=('', 269), dev=None, net=None, me=None, sockmodgen=Sock):
    TCPServer(root_instance, addr, dev, net, me, sockmodgen, micro_stream_request_handler)

class TCPClient(FakeRmt):
    '''This class implement a simple TCP RPC client'''

    def __init__(self,
                 host='localhost',
                 port=269,
                 net=None,
                 me=None,
                 sockmodgen=Sock,
                 xtimemod=xtime):

        self.host = host
        self.port = port

        self.xtime = xtimemod

        self.sockfactory = sockmodgen
        self.net = net
        self.me = me
        self.connected = False

        FakeRmt.__init__(self)

    def rpc_call(self, func_name, params):
        '''Performs a rpc call

        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

        while not self.connected:
            self.connect()
            self.xtime.swait(500)

        data = rencode.dumps((func_name, params))
        self.socket.sendall(_data_pack(data))

        recv_encoded_data = _data_unpack_from_stream_socket(self.socket)
        if not recv_encoded_data:
            raise RPCNetError('Connection closed before reply')
        recv_data = rencode.loads(recv_encoded_data)
        print recv_data
        logging.debug('Recvd data: %s' % str(recv_data))

        # Handling errors
        # I receive a message with the following format:
        #     ('rmt_error', message_error)
        # where message_error is a string
        if isinstance(recv_data, tuple) and recv_data[0] == 'rmt_error':
            raise RPCError(recv_data[1])

        return recv_data

    def connect(self):
        socket = self.sockfactory(net=self.net, me=self.me)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.socket.connect((self.host, self.port))
        except socket.error, e:
            pass
        else:
            self.connected = True

    def close(self):
        self.socket.close()
        self.connected = False

    def rmt(self, func_name, *params):
        return self.rpc_call(func_name, params)

    def __del__(self):
        if self.connected:
            self.close()


def dgram_request_handler(sock, clientaddr, packet, dev, rpcdispatcher):
    '''RPC stream request handler class

    Handles all request and try to decode them.
    '''
    caller = CallerInfo(clientaddr[0], clientaddr[1], dev, sock)
    #logging.debug('UDP packet from %s, dev %s', clientaddr, dev)
    try:
        data = _data_unpack_from_buffer(packet)
        #logging.debug('Handling data: %s', data)
        response = rpcdispatcher.marshalled_dispatch(caller, data)
    except RPCError:
        logging.debug('An error occurred during request handling')

def micro_dgram_request_handler(sock, clientaddr, packet, dev, rpcdispatcher):
    micro(dgram_request_handler, (sock, clientaddr, packet, dev, rpcdispatcher))

def UDPServer(root_instance, addr=('', 269), dev=None, net=None, me=None,
                sockmodgen=Sock, requestHandler=dgram_request_handler):
    '''This function implement a simple Rpc UDP server

    *WARNING*
    If the message to be received is greater than the buffer
    size, it will be lost! (buffer size is 8Kb)
    Use UDP RPC only for small calls, i.e. if the arguments passed to the
    remote function are small when packed.
    *WARNING*
    '''

    rpcdispatcher=RPCDispatcher(root_instance)
    socket=sockmodgen(net, me)
    s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    while True:
        message, address = s.recvfrom(8192)
        requestHandler(s, address, message, dev, rpcdispatcher)

@microfunc(True)
def MicroUDPServer(root_instance, addr=('', 269), dev=None, net=None, me=None, sockmodgen=Sock):
    UDPServer(root_instance, addr, dev, net, me, sockmodgen, micro_dgram_request_handler)

class BcastClient(FakeRmt):
    '''This class implement a simple Broadcast RPC client

    *WARNING*
    If the message to be received by the remote side is greater than the buffer
    size, it will be lost! (buffer size is 8Kb)
    Use UDP RPC only for small calls, i.e. if the arguments passed to the
    remote function are small when packed.
    *WARNING*
    '''

    def __init__(self, devs=[], port=269, net=None, me=None, sockmodgen=Sock, xtimemod=xtime):
        """
        devs:  list of devices where to send the broadcast calls
        If devs=[], the msg calls will be sent through all the available
        devices"""

        FakeRmt.__init__(self)
        self.socket=sockmodgen(net, me)
        self.xtime = xtimemod

        self.port = port

        self.devs = devs
        self.dev_sk = {}
        self.create_sockets()

    def create_sockets(self):
        socket = self.socket
        for d in self.devs:
            self.dev_sk[d] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.connected = False

    def rpc_call(self, func_name, params):
        '''Performs a rpc call

        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

        while not self.connected:
            self.connect()
            self.xtime.swait(500)

        data = rencode.dumps((func_name, params))
        self.send(_data_pack(data))

    def send(self, data):
        for d, sk in self.dev_sk.iteritems():
            sk.sendto(data, ('<broadcast>', self.port))

    def connect(self):
        for d, sk in self.dev_sk.iteritems():
            sk_bindtodevice(sk, d)
            sk_set_broadcast(sk)
            sk.connect(('<broadcast>', self.port))
        self.connected = True

    def close(self):
        for d, sk in self.dev_sk.iteritems():
            sk.close()
        self.connected = False

    def reset(self):
        self.close()
        self.create_sockets()

    def rmt(self, func_name, *params):
        self.rpc_call(func_name, params)

    def __del__(self):
        if self.connected:
            self.close()
