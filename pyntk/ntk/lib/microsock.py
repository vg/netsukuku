#---
# This source code is from stackless examples, but it has been modified a bit.
#---


#
# Stackless compatible socket module:
#
# Author: Richard Tew <richard.m.tew@gmail.com>
#
# This code was written to serve as an example of Stackless Python usage.
# Feel free to email me with any questions, comments, or suggestions for
# improvement.
#
# This wraps the asyncore module and the dispatcher class it provides in order
# write a socket module replacement that uses channels to allow calls to it to
# block until a delayed event occurs.
#
# Not all aspects of the socket module are provided by this file.  Examples of
# it in use can be seen at the bottom of this file.
#
# NOTE: Versions of the asyncore module from Python 2.4 or later include bug
#       fixes and earlier versions will not guarantee correct behaviour.
#       Specifically, it monitors for errors on sockets where the version in
#       Python 2.3.3 does not.
#

# Possible improvements:
# - More correct error handling.  When there is an error on a socket found by
#   poll, there is no idea what it actually is.
# - Launching each bit of incoming data in its own tasklet on the recvChannel
#   send is a little over the top.  It should be possible to add it to the
#   rest of the queued data

import asyncore
import socket as stdsocket # We need the "socket" name for 
                           # the function we export.
import time as stdtime

from ntk.lib.log import get_stackframes
from ntk.lib.log import logger as logging
from ntk.lib.micro import Channel, micro, allmicro_run, micro_block
from ntk.wrap.xtime import swait, time


# If we are to masquerade as the socket module, 
# we need to provide the constants.
if "__all__" in stdsocket.__dict__:
    __all__ = stdsocket.__dict__
    for k, v in stdsocket.__dict__.iteritems():
        if k in __all__:
            globals()[k] = v
else:
    for k, v in stdsocket.__dict__.iteritems():
        if k.upper() == k:
            globals()[k] = v
    error = stdsocket.error
    timeout = stdsocket.timeout
    # WARNING: this function blocks and is not thread safe.
    # The only solution is to spawn a thread to handle all
    # getaddrinfo requests.  Implementing a stackless DNS
    # lookup service is only second best as getaddrinfo may
    # use other methods.
    getaddrinfo = stdsocket.getaddrinfo

# urllib2 apparently uses this directly.  We need to cater for that.
_fileobject = stdsocket._fileobject

# Someone needs to invoke asyncore.poll() regularly to keep the socket
# data moving.  The "ManageSockets" function here is a simple example
# of such a function.  It is started by StartManager(), which uses the
# global "managerRunning" to ensure that no more than one copy is
# running.
#
# If you think you can do this better, register an alternative to
# StartManager using stacklesssocket_manager().  Your function will be
# called every time a new socket is created; it's your responsibility
# to ensure it doesn't start multiple copies of itself unnecessarily.
#

managerRunning = False

class MicrosockTimeout(Exception):
    pass

class WaitingSocketsManager(object):
    operation_accept = 1
    operation_recvfrom = 2
    
    def __init__(self):
        self.tuplelist = []

    def add(self, sock, operation, timeout):
        """Adds a socket with an operation and an expiration time."""
        def time_from_tuple(x):
            return x[2]

        expires = time() + timeout
        self.tuplelist.append((sock, operation, expires))
        self.tuplelist.sort(key = time_from_tuple)

    def remove(self, sock):
        """Removes a socket from the list."""
        def check(x):
            return x[0] == sock
        self.tuplelist = [t for t in self.tuplelist if not check(t)]

    def get_next(self):
        """Gets the first of sockets expired.
           Returns a tuple (sock, operation), or None.
           Also removes the returned sock from the list."""
        ret = None
        if self.tuplelist:
            if self.tuplelist[0][2] <= time():
                ret = self.tuplelist.pop(0)
                ret = (ret[0], ret[1])
        return ret

expiring_sockets = WaitingSocketsManager()

def ManageSockets():
    global managerRunning

    while len(asyncore.socket_map):
        # Check the sockets for activity.
        asyncore.poll(0.05)
        # Check if we have timed out operations.
        ret = expiring_sockets.get_next()
        if ret is not None:
            sock, operation = ret
            if operation == WaitingSocketsManager.operation_accept:
                # timed out accept
                if sock.acceptChannel and sock.acceptChannel.balance < 0:
                    sock.acceptChannel.send(MicrosockTimeout())
            elif operation == WaitingSocketsManager.operation_recvfrom:
                # timed out recvfrom
                if sock.recvChannel and sock.recvChannel.balance < 0:
                    sock.recvChannel.send(MicrosockTimeout())
            else:
                pass  # timeout not honored. we should signal that.

        # Yield to give other tasklets a chance to be scheduled.
        micro_block()

    managerRunning = False

def StartManager():
    global managerRunning
    if not managerRunning:
        managerRunning = True
        micro(ManageSockets)

_manage_sockets_func = StartManager

def stacklesssocket_manager(mgr):
    global _manage_sockets_func
    _manage_sockets_func = mgr

#
# Replacement for standard socket() constructor.
#
def socket(family=AF_INET, type=SOCK_STREAM, proto=0):
    global managerRunning

    currentSocket = stdsocket.socket(family, type, proto)
    ret = stacklesssocket(currentSocket)
    # Ensure that the sockets actually work.
    _manage_sockets_func()
    #stacktrace = get_stackframes(back=1)
    #logging.log(logging.ULTRADEBUG, 'created dispatcher ' + 
    #                      ret.dispatcher.__repr__() + ' in ' + stacktrace)
    #logging.log(logging.ULTRADEBUG, 'asyncore map: ' + 
    #                     str(asyncore.socket_map))
    return ret

# This is a facade to the dispatcher object.
# It exists because asyncore's socket map keeps a bound reference to
# the dispatcher and hence the dispatcher will never get gc'ed.
#
# The rest of the world sees a 'stacklesssocket' which has no cycles
# and will be gc'ed correctly

class stacklesssocket(object):
    def __init__(self, sock):
        self.sock = sock
        self.dispatcher = dispatcher(sock)

    def __getattr__(self, name):
        # Forward nearly everything to the dispatcher
        if not name.startswith("__"):
            # I don't like forwarding __repr__
            return getattr(self.dispatcher, name)

    def __setattr__(self, name, value):
        if name == "wrap_accept_socket":
            # We need to pass setting of this to the dispatcher.
            self.dispatcher.wrap_accept_socket = value
        else:
            # Anything else gets set locally.
            object.__setattr__(self, name, value)

    def __del__(self):
        try:
            #stacktrace = get_stackframes(back=1)
            #logging.log(logging.ULTRADEBUG, '**removed** dispatcher ' + 
            #             self.dispatcher.__repr__() + ' in ' + stacktrace)
            #logging.log(logging.ULTRADEBUG, 'asyncore map: ' + 
            #             str(asyncore.socket_map))
            # Close dispatcher if it isn't already closed
            if self.dispatcher.fileno() is not None:
                try:
                    self.dispatcher.close()
                finally:
                    self.dispatcher = None
        except stdsocket.error, e:
            raise

    # Catch this one here to make gc work correctly.
    # (Consider if stacklesssocket gets gc'ed before the _fileobject)
    def makefile(self, mode='r', bufsize=-1):
        return stdsocket._fileobject(self, mode, bufsize)


class dispatcher(asyncore.dispatcher):
    connectChannel = None
    acceptChannel = None
    recvChannel = None
    _receiving = False
    _justConnected = False


    def __init__(self, sock):
        # This is worth doing.  I was passing in an invalid socket which was
        # an instance of dispatcher and it was causing tasklet death.
        if not isinstance(sock, stdsocket.socket):
            raise StandardError("Invalid socket passed to dispatcher")
        asyncore.dispatcher.__init__(self, sock)

        # if self.socket.type == SOCK_DGRAM:
        #    self.dgramRecvChannels = {}
        #    self.dgramReadBuffers = {}
        #else:
        self.recvChannel = Channel(micro_send=True)
        self.readBufferString = ''
        self.readBufferList = []

        self.sendBuffer = ''
        self.send_has_exception = None
        self.send_will_handle_exception = False
        self.sendToBuffers = []

        self.maxreceivebuf=65536

    def readable(self):
        if self.socket.type != SOCK_DGRAM and self.connected:
            return self._receiving
        return asyncore.dispatcher.readable(self)

    def writable(self):
        if self.socket.type != SOCK_DGRAM and not self.connected:
            return True
        return len(self.sendBuffer) or len(self.sendToBuffers)

    def accept(self, timeout = None):
        if timeout is not None:
            expiring_sockets.add(self, WaitingSocketsManager.operation_accept,
                                 timeout)
        if not self.acceptChannel:
            self.acceptChannel = Channel(micro_send=True)
        ret = self.acceptChannel.recv()
        self.acceptChannel = None
        if timeout is not None:
            if isinstance(ret,MicrosockTimeout):
                raise ret
            else:
                expiring_sockets.remove(self)
        return ret


    def connect(self, address):
        asyncore.dispatcher.connect(self, address)
        # UDP sockets do not connect.

        if self.socket.type != SOCK_DGRAM and not self.connected:
            if not self.connectChannel:
                # Prefer the sender.  Do not block when sending, given that
                # there is a tasklet known to be waiting, this will happen.
                self.connectChannel = Channel(prefer_sender=True)
            ret = self.connectChannel.recv()
            self.connectChannel = None   # To make sure that we don't do again
                                         # a 'send' on this channel
            # Handling errors
            if isinstance(ret, Exception):
                raise ret


    def send(self, data):
        self.sendBuffer += data
        micro_block()
        return len(data)

    def sendall(self, data):
        self.send_has_exception = None
        self.send_will_handle_exception = True
        try:
            self.sendBuffer += data
            while True:
                # Handling errors
                if self.send_has_exception:
                    # self.sendBuffer has been already cleared.
                    raise self.send_has_exception
                if self.sendBuffer == '':
                    break
                micro_block()
                stdtime.sleep(0.001)
        finally:
            self.send_will_handle_exception = False
        return len(data)

    def sendto(self, sendData, sendAddress):
        waitChannel = None
        for idx, (data, address, channel, sentBytes, waiting_tasklets) in \
                   enumerate(self.sendToBuffers):

            if address == sendAddress:
                self.sendToBuffers[idx] = (data + sendData, address, channel, 
                                           sentBytes, waiting_tasklets + 1)
                waitChannel = channel
                break
        if waitChannel is None:
            waitChannel = Channel(micro_send=True)
            self.sendToBuffers.append((sendData, sendAddress, waitChannel, 0, 
                                       1))

        ret = waitChannel.recv()
        # Handling errors
        # I receive a message with the following format:
        #     ('rmt_error', message_error)
        # where message_error is a string
        if isinstance(ret, tuple) and ret[0] == 'microsock_error':
            raise error(ret[1])
        return ret

    # Read at most byteCount bytes.
    def recv(self, byteCount):
        self._receiving = True
        try:
            self.maxreceivebuf=byteCount
            if len(self.readBufferString) < byteCount:
                # If our buffer is empty, we must block for more data we also
                # aggressively request more if it's available.
                if len(self.readBufferString) == 0 or \
                   self.recvChannel.balance > 0:
                    self.readBufferString += self.recvChannel.recv()
            # Disabling this because I believe it is the onus of the 
            # application to be aware of the need to run the scheduler 
            # to give other tasklets leeway to run.
            # stackless.schedule()
            ret = self.readBufferString[:byteCount]
            self.readBufferString = self.readBufferString[byteCount:]
            return ret
        finally:
            self._receiving = False

    def recvfrom(self, byteCount, timeout = None):
        self.maxreceivebuf=byteCount
        if timeout is not None:
            expiring_sockets.add(self, 
                        WaitingSocketsManager.operation_recvfrom, timeout)
        ret = self.recvChannel.recv()
        if timeout is not None:
            if isinstance(ret,MicrosockTimeout):
                raise ret
            else:
                expiring_sockets.remove(self)
        return ret

    def close(self):
        asyncore.dispatcher.close(self)
        self.connected = False
        self.accepting = False
        self.sendBuffer = ''  # breaks the loop in sendall

        # Clear out all the channels with relevant errors.

        if self.acceptChannel is not None:
            while self.acceptChannel and self.acceptChannel.balance < 0:
                self.acceptChannel.send_exception(error, 9, 
                                                  'Bad file descriptor')
        if self.connectChannel is not None:
            while self.connectChannel and self.connectChannel.balance < 0:
                self.connectChannel.send_exception(error, 10061, 
                                                   'Connection refused')
        if self.recvChannel is not None:
            while self.recvChannel and self.recvChannel.balance < 0:
                # The closing of a socket is indicted by receiving nothing.  
                # The exception would have been sent if the server was killed,
                # rather than closed down gracefully.
                self.recvChannel.ch.send("")
                #self.recvChannel.send_exception(error, 10054, 
                #                              'Connection reset by peer')

    # asyncore doesn't support this.  Why not?
    def fileno(self):
            # XXX: self.socket.fileno() raises a Bad file descriptor error.
            #      Therefore, we're using _fileno as a hack. This has to be
            #      cleaned.
            # return self.socket.fileno() 
        return self._fileno

    def handle_accept(self):
        if self.acceptChannel and self.acceptChannel.balance < 0:
            currentSocket, clientAddress = asyncore.dispatcher.accept(self)
            currentSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            # Give them the asyncore based socket, not the standard one.
            currentSocket = self.wrap_accept_socket(currentSocket)
            self.acceptChannel.send((currentSocket, clientAddress))

    # Inform the blocked connect call that the connection has been made.
    def handle_connect(self):
        self._justConnected = True
        if self.socket.type != SOCK_DGRAM and self.connectChannel is not None:
            self.connectChannel.send(None)

    # Asyncore says its done but self.readBuffer may be non-empty
    # so can't close yet.  Do nothing and let 'recv' trigger the close.
    def handle_close(self):
        pass

    # Some error, just close the channel and let that raise errors to
    # blocked calls.
    def handle_expt(self):
        # if we were connecting, report error to method connect
        if self.connectChannel is not None:
            err = self.socket.getsockopt(stdsocket.SOL_SOCKET, 
                                         stdsocket.SO_ERROR)
            import os
            from errno import errorcode
            msg = os.strerror(err)
            if msg == 'Unknown error':
                msg = errorcode[err]
            self.connectChannel.send(stdsocket.error(err, msg))
        if self._justConnected:
            self._justConnected = False
        self.close()

    def handle_read(self):
        if self._justConnected:
            self._justConnected = False
            return
        try:
            if self.socket.type == SOCK_DGRAM:
                ret, address = self.socket.recvfrom(self.maxreceivebuf)
                self.recvChannel.send((ret, address))
            else:
                ret = asyncore.dispatcher.recv(self, self.maxreceivebuf)
                # Not sure this is correct, but it seems to give the
                # right behaviour.  Namely removing the socket from
                # asyncore.
                if not ret:
                    self.close()
                self.recvChannel.send(ret)
        except stdsocket.error, err:
            # XXX Is this correct?
            # If there's a read error assume the connection is
            # broken and drop any pending output
            if self.sendBuffer:
                self.sendBuffer = ""
            # Why can't I pass the 'err' by itself?
            self.recvChannel.ch.send_exception(stdsocket.error, err)

    def handle_write(self):
        if self._justConnected:
            self._justConnected = False
            return
        if len(self.sendBuffer):
            try:
                sentBytes = asyncore.dispatcher.send(self, 
                                                     self.sendBuffer[:512])
            except Exception as e:
                logging.log(logging.ULTRADEBUG, 'microsock: dispatcher ' + 
                            self.__repr__() + ' in handle_write raised ' + 
                            str(type(e)) + ' - ' + str(e))
                self.sendBuffer = ''
                if self.send_will_handle_exception:
                    self.send_has_exception = e
                else:
                    logging.warning('An exception was raised in '
                                    'microsock.dispatcher.handle_write raised'
                                    ' that won\'t be reported.')
            else:
                self.sendBuffer = self.sendBuffer[sentBytes:]
        elif len(self.sendToBuffers):
            data, address, channel, oldSentBytes, waiting_tasklets = \
            self.sendToBuffers[0]
            try:
                sentBytes = self.socket.sendto(data, address)
            except Exception as e:
                logging.log(logging.ULTRADEBUG, 'microsock: dispatcher ' + 
                            self.__repr__() + ' in handle_write raised ' + 
                            str(type(e)) + ' - ' + str(e))
                del self.sendToBuffers[0]
                for i in xrange(waiting_tasklets):
                    channel.send(('microsock_error', str(e)))
            else:
                totalSentBytes = oldSentBytes + sentBytes
                if len(data) > sentBytes:
                    self.sendToBuffers[0] = data[sentBytes:], address, 
                    channel, totalSentBytes, waiting_tasklets
                else:
                    del self.sendToBuffers[0]
                    for i in xrange(waiting_tasklets):
                        channel.send(totalSentBytes)


    # In order for incoming connections to be stackless compatible,
    # they need to be wrapped by an asyncore based dispatcher subclass.
    def wrap_accept_socket(self, currentSocket):
        return stacklesssocket(currentSocket)
