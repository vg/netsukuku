##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# (c) Copyright 2009 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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

from random import randint

import ntk.lib.rencode as rencode
import time

from ntk.lib.log import logger as logging
from ntk.lib.log import log_exception_stacktrace
from ntk.lib.micro import  (micro, microfunc, micro_block, Channel, 
                            MicrochannelTimeout)
from ntk.lib.microsock import MicrosockTimeout
from ntk.network.inet import sk_set_broadcast, sk_bindtodevice
from ntk.wrap import xtime as xtime
from ntk.wrap.sock import Sock
from ntk.core.status import ZombieException


class RPCError(Exception): pass
class RPCNetError(RPCError): pass
class RPCFuncNotRemotable(RPCError): pass


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
            logging.warning("func_get: "+str(func_name) + 
                            " AttributeError, not remotable.")
            return None

        logging.warning("func_get: "+str(func_name) + " not remotable.")
        return None

    def _dispatch(self, caller, func_name, params):
        if not 'radar' in func_name:
            logging.log(logging.ULTRADEBUG, "_dispatch: "+func_name+
                        "("+str(params)+")")
        func = None
        rpc_caller_present = False
        if func_name == 'UDP_got_keepalive':
            global UDP_got_keepalive
            func = UDP_got_keepalive
            rpc_caller_present = '_rpc_caller' in func.func_code.co_varnames
        elif func_name == 'UDP_got_reply':
            global UDP_got_reply
            func = UDP_got_reply
            rpc_caller_present = '_rpc_caller' in func.func_code.co_varnames
        else:
            func = self.func_get(func_name)
            rpc_caller_present = '_rpc_caller' in \
                                  func.im_func.func_code.co_varnames
        if func is None:
            raise RPCFuncNotRemotable('Function %s is not remotable' % 
                                      func_name)
        try:
            if rpc_caller_present:
                ret = func(caller, *params)
                return ret
            else:
                ret = func(*params)
                return ret
        except Exception, e:
        # I propagate all exceptions to `dispatch'
            raise

    def dispatch(self, caller, func, params):
        try:
            response = self._dispatch(caller, func, params)
        except Exception, e:
            if isinstance(e, ZombieException):
                logging.debug('Zombie Exception raised in ' \
                              + str(func) + '(' + str(params) + ').')
            else:
                logging.error("Uncaught exception in a remotable function")
                logging.error("  The function has been called like this: %s(%s)" %
                              (func, params))
                log_exception_stacktrace(e)
            response = ('rmt_error', str(e))
        if not 'radar' in func:
            pass #logging.debug("dispatch response: "+str(response))
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
            pass #logging.debug(e)
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

tcp_servers_running_instances = []
tcp_stopping_servers = False
udp_servers_running_instances = []
udp_stopping_servers = False

def stream_request_handler(sock, clientaddr, dev, rpcdispatcher):
    pass #logging.debug('Connected from %s, dev %s', clientaddr, dev)
    caller = CallerInfo(clientaddr[0], clientaddr[1], dev, sock)
    while True:
        try:
            data = _data_unpack_from_stream_socket(sock)
            if not data: break
            pass #logging.debug('Handling data: %s', data)
            response = rpcdispatcher.marshalled_dispatch(caller, data)
            pass #logging.debug('Response: %s', response)
        except RPCError:
            pass #logging.debug('An error occurred during request handling')

        sock.send(_data_pack(response))
        #self.request.close()
        pass #logging.debug('Response sent')
    sock.close()

def micro_stream_request_handler(sock, clientaddr, dev, rpcdispatcher):
    micro(stream_request_handler, (sock, clientaddr, dev, rpcdispatcher))

def TCPServer(root_instance, addr=('', 269), dev=None, net=None, me=None,
                sockmodgen=Sock, request_handler=stream_request_handler):
    global tcp_stopping_servers
    global tcp_servers_running_instances
 
    this_server_id = randint(0, 2**32-1)
    tcp_servers_running_instances.append('TCP' + str(this_server_id))
    socket=sockmodgen(net, me)
    s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(8)
    rpcdispatcher=RPCDispatcher(root_instance)
    while not tcp_stopping_servers:
        try:
            sock, clientaddr = s.accept(timeout = 1000)
        except MicrosockTimeout:
            pass
        else:
            request_handler(sock, clientaddr, dev, rpcdispatcher)

    tcp_servers_running_instances.remove('TCP' + str(this_server_id))
    if not tcp_servers_running_instances:
        tcp_stopping_servers = False

@microfunc(True)
def MicroTCPServer(root_instance, addr=('', 269), dev=None, net=None, 
                   me=None, sockmodgen=Sock):
    TCPServer(root_instance, addr, dev, net, me, sockmodgen, 
              micro_stream_request_handler)

def stop_tcp_servers():
    """ Stop the TCP servers """
    global tcp_stopping_servers
    global tcp_servers_running_instances
    
    if tcp_servers_running_instances:
        tcp_stopping_servers = True
        while tcp_stopping_servers:
            time.sleep(0.001)
            micro_block()

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
        self.calling = False

        FakeRmt.__init__(self)

    def rpc_call(self, func_name, params):
        '''Performs a rpc call

        @param func_name: name of the remote callable
        @param params: a tuple of arguments to pass to the remote callable
        '''

        # Let's make sure that we'll send the actual content of the params.
        # Evaluate them before possibly passing the schedule.
        data = rencode.dumps((func_name, params))

        while not self.connected:
            self.connect()
            self.xtime.swait(500)

        while self.calling:
            # go away waiting that the previous 
            # rpc_call is accomplished
            time.sleep(0.001)
            micro_block()

        # now other microthread cannot call make an RPC call
        # until the previous call has not received the reply
        self.calling = True

        self.socket.sendall(_data_pack(data))
        recv_encoded_data = _data_unpack_from_stream_socket(self.socket)

        self.calling = False
        # let other calls work

        if not recv_encoded_data:
            raise RPCNetError('Connection closed before reply')

        recv_data = rencode.loads(recv_encoded_data)

        pass #logging.debug("Recvd data: "+str(recv_data))

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
        lenpacket = len(packet)
        if lenpacket > 4000: logging.warning('CAUTION! Handling UDP packet '
                                             'of %s bytes.' % lenpacket)
        elif lenpacket > 3000: logging.debug('WARNING!!! Handling UDP packet '
                                             'of %s bytes.' % lenpacket)
        elif lenpacket > 1024: logging.debug('Handling UDP packet of %s '
                                             'bytes.' % lenpacket)
        data = _data_unpack_from_buffer(packet)
        response = rpcdispatcher.marshalled_dispatch(caller, data)
        #logging.debug('Dispatched some data')
    except RPCError:
        logging.warning('An error occurred during request handling')

def micro_dgram_request_handler(sock, clientaddr, packet, dev, rpcdispatcher):
    micro(dgram_request_handler, (sock, clientaddr, packet, dev, 
                                  rpcdispatcher))

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
    global udp_stopping_servers
    global udp_servers_running_instances

    this_server_id = randint(0, 2**32-1)
    udp_servers_running_instances.append('UDP' + str(this_server_id))
    rpcdispatcher=RPCDispatcher(root_instance)
    socket=sockmodgen(net, me)
    s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sk_bindtodevice(s, dev)
    s.bind(addr)
    
    while not udp_stopping_servers:
        try:
            message, address = s.recvfrom(8192, timeout = 1000)
        except MicrosockTimeout:
            pass
        else:
            requestHandler(s, address, message, dev, rpcdispatcher)
    udp_servers_running_instances.remove('UDP' + str(this_server_id))
    if not udp_servers_running_instances:
        udp_stopping_servers = False


@microfunc(True)
def MicroUDPServer(root_instance, addr=('', 269), dev=None, net=None, 
                   me=None, sockmodgen=Sock):
    UDPServer(root_instance, addr, dev, net, me, sockmodgen, 
              micro_dgram_request_handler)

def stop_udp_servers():
    """ Stop the UDP servers """
    global udp_stopping_servers
    global udp_servers_running_instances

    if udp_servers_running_instances:
        udp_stopping_servers = True
        while udp_stopping_servers:
            time.sleep(0.001)
            micro_block()

class BcastClient(FakeRmt):
    '''This class implement a simple Broadcast RPC client

    *WARNING*
    If the message to be received by the remote side is greater than the 
    buffer size, it will be lost! (buffer size is 8Kb)
    Use UDP RPC only for small calls, i.e. if the arguments passed to the
    remote function are small when packed.
    *WARNING*
    '''

    def __init__(self, devs=[], port=269, net=None, me=None, sockmodgen=Sock, 
                 xtimemod=xtime):
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

        # Let's make sure that we'll send the actual content of the params.
        # Evaluate them before possibly passing the schedule.
        data = rencode.dumps((func_name, params))

        while not self.connected:
            self.connect()
            self.xtime.swait(500)

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

UDP_caller_ids = {}
def UDP_call(callee_nip, callee_netid, devs, func_name, args=()):
    """Use a BcastClient to call 'func_name' to 'callee_nip' on the LAN 
    via UDP broadcast."""

    logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + ' to ' + 
                str(callee_nip) + ' of ' + str(callee_netid) + 
                ' on the LAN via UDP broadcast.')
    bcastclient = None
    try:
        bcastclient = BcastClient(devs=devs, xtimemod=xtime)
        logging.log(logging.ULTRADEBUG, 'created BcastClient with devs = ' + 
                    str(devs))
    except:
        raise RPCError('Couldn\'t create BcastClient.')
    caller_id = randint(0, 2**32-1)
    UDP_caller_ids[caller_id] = Channel()
    for i in xrange(5):
        try:
            exec('bcastclient.' + func_name + '(caller_id, callee_nip, '
                                              'callee_netid, *args)')
            if i == 0:
                logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + 
                            ' to ' + str(callee_nip) + ' of ' + 
                            str(callee_netid) + ' done. Waiting reply...')
            else:
                logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + 
                            ' to ' + str(callee_nip) + ' of ' + 
                            str(callee_netid) + ' retried (' + str(i) + 
                            '). Waiting reply...')
            ret = None
            try:
                while True:
                    ret = UDP_caller_ids[caller_id].recv(timeout=3000)
                    # Handling keepalives
                    # I receive a message with the following format:
                    #     ('rmt_keepalive', )
                    if isinstance(ret, tuple) and ret[0] == 'rmt_keepalive':
                        logging.log(logging.ULTRADEBUG, 'Calling ' + 
                                    func_name + ' to ' + str(callee_nip) + 
                                    ' of ' + str(callee_netid) + 
                                    ' got REFRESHed by a keepalive.')
                    else:
                        break
            except MicrochannelTimeout:
                raise RPCNetError('Timeout')
            break
        except Exception as e:
            if i == 4:
                raise e
        # Try again
    logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + ' to ' + 
                str(callee_nip) + ' of ' + str(callee_netid) + ' got reply.')
    # Handling errors
    # I receive a message with the following format:
    #     ('rmt_error', message_error)
    # where message_error is a string
    if isinstance(ret, tuple) and ret[0] == 'rmt_error':
        raise RPCError(ret[1])
    return ret

UDP_caller_ids_keeping_alive = {}
def UDP_send_keepalive_forever_start(_rpc_caller, caller_id, interval=800):
    """Starts sending a keepalive each interval"""
    UDP_caller_ids_keeping_alive[caller_id] = None
    logging.log(logging.ULTRADEBUG, 'keepalive_forever ' + str(caller_id))
    UDP_send_keepalive_forever_start_micro(_rpc_caller, caller_id, interval)

@microfunc(True)
def UDP_send_keepalive_forever_start_micro(_rpc_caller, caller_id, interval):
    """Sends a keepalive each interval"""
    while True:
        xtime.swait(interval)
        if caller_id in UDP_caller_ids_keeping_alive:
            UDP_send_keepalive(_rpc_caller, caller_id)
        else:
            logging.log(logging.ULTRADEBUG, 'keepalive_forever ' + 
                        str(caller_id) + ': stopped.')
            break

def UDP_send_keepalive_forever_stop(caller_id):
    """Ends sending a keepalive each interval"""
    logging.log(logging.ULTRADEBUG, 'keepalive_forever ' + str(caller_id) + 
                ': request stop if present.')
    if caller_id in UDP_caller_ids_keeping_alive:
        logging.log(logging.ULTRADEBUG, 'keepalive_forever ' + 
                    str(caller_id) + ': request stop.')
        del UDP_caller_ids_keeping_alive[caller_id]
        logging.log(logging.ULTRADEBUG, 'keepalive_forever ' + str(caller_id) 
                    + ': stop requested.')

def UDP_send_keepalive(_rpc_caller, caller_id):
    """Send a keepalive"""
    logging.log(logging.ULTRADEBUG, 'Sending keepalive to id ' + 
                str(caller_id) + ' through ' + str(_rpc_caller.dev))
    exec('BcastClient(devs=[_rpc_caller.dev], xtimemod=xtime).'
         'UDP_got_keepalive(caller_id)')

def UDP_got_keepalive(_rpc_caller, caller_id):
    """Receives keepalive from a UDP_call."""
    logging.log(logging.ULTRADEBUG, 'Seen a keepalive from a UDP_call.')
    if caller_id in UDP_caller_ids:
        # This keepalive is for me.
        logging.log(logging.ULTRADEBUG, ' ...it is for me!')
        chan = UDP_caller_ids[caller_id]
        if chan.balance < 0:
            logging.log(logging.ULTRADEBUG, ' ...sending through channel')
            chan.send(('rmt_keepalive', ))
            # We have passed the schedule to the receiving channel, 
            # so don't put loggings after this, they would just 
            # confuse the reader.
    else:
        logging.log(logging.ULTRADEBUG, ' ...it is not for me.')

def UDP_send_reply(_rpc_caller, caller_id, ret):
    """Send a reply"""

    logging.log(logging.ULTRADEBUG, 'Sending reply to id ' + str(caller_id) + 
                ' through ' + str(_rpc_caller.dev))
    exec('BcastClient(devs=[_rpc_caller.dev], xtimemod=xtime).UDP_got_reply'
         '(caller_id, ret)')

def UDP_got_reply(_rpc_caller, caller_id, ret):
    """Receives reply from a UDP_call."""
    
    logging.log(logging.ULTRADEBUG, 'Seen a reply to UDP_call.')
    if caller_id in UDP_caller_ids:
        # This reply is for me.
        logging.log(logging.ULTRADEBUG, ' ...it is for me!')
        chan = UDP_caller_ids[caller_id]
        if chan.balance < 0:
            logging.log(logging.ULTRADEBUG, ' ...sending through channel')
            chan.send(ret)
            # We have passed the schedule to the receiving channel, 
            # so don't put loggings after this, they would just 
            # confuse the reader.
            del UDP_caller_ids[caller_id]
    else:
        logging.log(logging.ULTRADEBUG, ' ...it is not for me.')

def UDP_broadcast_call(devs, func_name, args=()):
    """Use a BCastClient to call 'func_name' to every host (except myself)
       reached directly through 'devs'."""

    # Note: The caller must provide himself the list of devs that are
    # managed by netsukuku.
    # func_name is a remotable procedure (usually a microfunc)

    logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + 
                ' to everybody via ' + str(devs))
    bcastclient = BcastClient(devs=devs, xtimemod=xtime)
    logging.log(logging.ULTRADEBUG, 'created BcastClient with devs = ' + 
                str(devs))
    caller_id = randint(0, 2**32-1)
    UDP_caller_ids[caller_id] = len(devs)
    exec('bcastclient.' + func_name + '(caller_id, *args)')
    logging.log(logging.ULTRADEBUG, 'Calling ' + func_name + ' done.')

def UDP_broadcast_got_call(_rpc_caller, caller_id):
    """Receives call from a UDP_broadcast_call.
       Returns True if I must execute the remotable procedure.
       Returns False if the call was done by me."""
    
    logging.log(logging.ULTRADEBUG, 'Seen a UDP_broadcast_call.')
    if caller_id in UDP_caller_ids:
        # This call is from me.
        logging.log(logging.ULTRADEBUG, 
                    ' ...it was from me! I will do nothing.')
        # Normally, I will "receive" a call from myself <n> times, the
        # number of managed NICs.
        UDP_caller_ids[caller_id] = UDP_caller_ids[caller_id] - 1
        if UDP_caller_ids[caller_id] == 0:
            del UDP_caller_ids[caller_id]
        return False
    else:
        logging.log(logging.ULTRADEBUG, ' ...it is for me, too.')
        return True

