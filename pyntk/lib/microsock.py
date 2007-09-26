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

from micro import Channel, micro, allmicro_run, micro_block
import asyncore
import socket as stdsocket # We need the "socket" name for the function we export.

# If we are to masquerade as the socket module, we need to provide the constants.
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

def ManageSockets():
    global managerRunning

    while len(asyncore.socket_map):
        # Check the sockets for activity.
        asyncore.poll(0.05)
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
        # Close dispatcher if it isn't already closed
        if self.dispatcher._fileno is not None:
            try:
                self.dispatcher.close()
            finally:
                self.dispatcher = None

    # Catch this one here to make gc work correctly.
    # (Consider if stacklesssocket gets gc'ed before the _fileobject)
    def makefile(self, mode='r', bufsize=-1):
        return stdsocket._fileobject(self, mode, bufsize)


class dispatcher(asyncore.dispatcher):
    connectChannel = None
    acceptChannel = None
    recvChannel = None

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
        self.sendToBuffers = []

    def writable(self):
        if self.socket.type != SOCK_DGRAM and not self.connected:
            return True
        return len(self.sendBuffer) or len(self.sendToBuffers)

    def accept(self):
        if not self.acceptChannel:
            self.acceptChannel = Channel(micro_send=True)
        return self.acceptChannel.recv()

    def connect(self, address):
        asyncore.dispatcher.connect(self, address)
        # UDP sockets do not connect.
        if self.socket.type != SOCK_DGRAM and not self.connected:
            if not self.connectChannel:
                # Prefer the sender.  Do not block when sending, given that
                # there is a tasklet known to be waiting, this will happen.
                self.connectChannel = Channel(prefer_sender=True)
            self.connectChannel.recv()

    def send(self, data):
        self.sendBuffer += data
        micro_block()
        return len(data)

    def sendall(self, data):
        # WARNING: this will busy wait until all data is sent
        # It should be possible to do away with the busy wait with
        # the use of a channel.
        self.sendBuffer += data
        while self.sendBuffer:
            micro_block()
        return len(data)

    def sendto(self, sendData, sendAddress):
        waitChannel = None
        for idx, (data, address, channel, sentBytes) in enumerate(self.sendToBuffers):
            if address == sendAddress:
                self.sendToBuffers[idx] = (data + sendData, address, channel, sentBytes)
                waitChannel = channel
                break
        if waitChannel is None:
            waitChannel = Channel(micro_send=True)
            self.sendToBuffers.append((sendData, sendAddress, waitChannel, 0))
        return waitChannel.recv()

    # Read at most byteCount bytes.
    def recv(self, byteCount):
        if len(self.readBufferString) < byteCount:
            # If our buffer is empty, we must block for more data we also
            # aggressively request more if it's available.
            if len(self.readBufferString) == 0 or self.recvChannel.ch.balance > 0:
                self.readBufferString += self.recvChannel.recv()
        # Disabling this because I believe it is the onus of the application
        # to be aware of the need to run the scheduler to give other tasklets
        # leeway to run.
        # stackless.schedule()
        ret = self.readBufferString[:byteCount]
        self.readBufferString = self.readBufferString[byteCount:]
        return ret

    def recvfrom(self, byteCount):
        ret = ""
        address = None
        while 1:
            while len(self.readBufferList):
                data, dataAddress = self.readBufferList[0]
                if address is None:
                    address = dataAddress
                elif address != dataAddress:
                    # They got all the sequential data from the given address.
                    return ret, address

                ret += data
                if len(ret) >= byteCount:
                    # We only partially used up this data.
                    self.readBufferList[0] = ret[byteCount:], address
                    return ret[:byteCount], address

                # We completely used up this data.
                del self.readBufferList[0]

            self.readBufferList.append(self.recvChannel.recv())

    def close(self):
        asyncore.dispatcher.close(self)
        self.connected = False
        self.accepting = False
        self.sendBuffer = None  # breaks the loop in sendall

        # Clear out all the channels with relevant errors.
        while self.acceptChannel and self.acceptChannel.ch.balance < 0:
            self.acceptChannel.send_exception(error, 9, 'Bad file descriptor')
        while self.connectChannel and self.connectChannel.ch.balance < 0:
            self.connectChannel.send_exception(error, 10061, 'Connection refused')
        while self.recvChannel and self.recvChannel.ch.balance < 0:
            # The closing of a socket is indicted by receiving nothing.  The
            # exception would have been sent if the server was killed, rather
            # than closed down gracefully.
            self.recvChannel.ch.send("")
            #self.recvChannel.send_exception(error, 10054, 'Connection reset by peer')

    # asyncore doesn't support this.  Why not?
    def fileno(self):
        return self.socket.fileno()

    def handle_accept(self):
        if self.acceptChannel and self.acceptChannel.ch.balance < 0:
            currentSocket, clientAddress = asyncore.dispatcher.accept(self)
            currentSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
            # Give them the asyncore based socket, not the standard one.
            currentSocket = self.wrap_accept_socket(currentSocket)
            self.acceptChannel.send((currentSocket, clientAddress))

    # Inform the blocked connect call that the connection has been made.
    def handle_connect(self):
        if self.socket.type != SOCK_DGRAM:
            self.connectChannel.send(None)

    # Asyncore says its done but self.readBuffer may be non-empty
    # so can't close yet.  Do nothing and let 'recv' trigger the close.
    def handle_close(self):
        pass

    # Some error, just close the channel and let that raise errors to
    # blocked calls.
    def handle_expt(self):
        self.close()

    def handle_read(self):
        try:
            if self.socket.type == SOCK_DGRAM:
                ret, address = self.socket.recvfrom(20000)
                self.recvChannel.send((ret, address))
            else:
                ret = asyncore.dispatcher.recv(self, 20000)
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
        if len(self.sendBuffer):
            sentBytes = asyncore.dispatcher.send(self, self.sendBuffer[:512])
            self.sendBuffer = self.sendBuffer[sentBytes:]
        elif len(self.sendToBuffers):
            data, address, channel, oldSentBytes = self.sendToBuffers[0]
            sentBytes = self.socket.sendto(data, address)
            totalSentBytes = oldSentBytes + sentBytes
            if len(data) > sentBytes:
                self.sendToBuffers[0] = data[sentBytes:], address, channel, totalSentBytes
            else:
                del self.sendToBuffers[0]
                channel.send(totalSentBytes)

    # In order for incoming connections to be stackless compatible,
    # they need to be wrapped by an asyncore based dispatcher subclass.
    def wrap_accept_socket(self, currentSocket):
        return stacklesssocket(currentSocket)
