##
# This file is part of Netsukuku
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

import sys
sys.path.append('..')
from ntk.lib.micro import micro, microfunc, allmicro_run, Channel

from ntk.wrap.sock import Sock
socket = Sock()
from socket import AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, SOCK_DGRAM
import struct



# Test code goes here.
testAddress = "127.0.0.1", 3000
info = -12345678
data = struct.pack("i", info)
dataLength = len(data)

print "creating listen socket"
def TestTCPServer(address, socketClass=None):
    global info, data, dataLength

    if not socketClass:
        socketClass = socket.socket

    listenSocket = socketClass(AF_INET, SOCK_STREAM)
    listenSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    listenSocket.bind(address)
    listenSocket.listen(5)

    NUM_TESTS = 2

    i = 1
    while i < NUM_TESTS + 1:
        # No need to schedule this tasklet as the accept should yield most
        # of the time on the underlying channel.
        print "waiting for connection test", i
        currentSocket, clientAddress = listenSocket.accept()
        print "received connection", i, "from", clientAddress

        if i == 1:
            currentSocket.close()
        elif i == 2:
            print "server test", i, "send"
            currentSocket.send(data)
            print "server test", i, "recv"
            if currentSocket.recv(4) != "":
                print "server recv(1)", i, "FAIL"
                break
            # multiple empty recvs are fine
            if currentSocket.recv(4) != "":
                print "server recv(2)", i, "FAIL"
                break
        else:
            currentSocket.close()

        print "server test", i, "OK"
        i += 1

    if i != NUM_TESTS+1:
        print "server: FAIL", i
    else:
        print "server: OK", i

    print "Done server"

def TestTCPClient(address, socketClass=None):
    global info, data, dataLength

    if not socketClass:
        socketClass = socket.socket

    # Attempt 1:
    clientSocket = socketClass()
    clientSocket.connect(address)
    print "client connection", 1, "waiting to recv"
    if clientSocket.recv(5) != "":
        print "client test", 1, "FAIL"
    else:
        print "client test", 1, "OK"

    # Attempt 2:
    clientSocket = socket.socket()
    clientSocket.connect(address)
    print "client connection", 2, "waiting to recv"
    s = clientSocket.recv(dataLength)
    if s == "":
        print "client test", 2, "FAIL (disconnect)"
    else:
        t = struct.unpack("i", s)
        if t[0] == info:
            print "client test", 2, "OK"
        else:
            print "client test", 2, "FAIL (wrong data)"

def TestMonkeyPatchUrllib(uri):
    # replace the system socket with this module
    import urllib  # must occur after monkey-patching!
    f = urllib.urlopen(uri)
    if not isinstance(f.fp._sock, socket.stacklesssocket):
        raise AssertionError("failed to apply monkeypatch")
    s = f.read()
    if len(s) != 0:
        print "Fetched", len(s), "bytes via replaced urllib"
    else:
        raise AssertionError("no text received?")

def TestMonkeyPatchUDP(address):
    # replace the system socket with this module
    def UDPServer(address):
        listenSocket = socket.socket(AF_INET, SOCK_DGRAM)
        listenSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        listenSocket.bind(address)

        i = 1
        cnt = 0
        while 1:
            #print "waiting for connection test", i
            #currentSocket, clientAddress = listenSocket.accept()
            #print "received connection", i, "from", clientAddress

            print "waiting to receive"
            t = listenSocket.recvfrom(256)
            cnt += len(t[0])
            print "received", t[0], cnt
            if cnt == 512:
                break

    def UDPClient(address):
        clientSocket = socket.socket(AF_INET, SOCK_DGRAM)
        # clientSocket.connect(address)
        print "sending 512 byte packet"
        sentBytes = clientSocket.sendto("-"+ ("*" * 510) +"-", address)
        print "sent 512 byte packet", sentBytes

    micro(UDPServer, (address,))
    micro(UDPClient, (address,))
    allmicro_run()

if len(sys.argv) == 2:
    if sys.argv[1] == "client":
        print "client started"
        TestTCPClient(testAddress, socket.socket)
        print "client exited"
    elif sys.argv[1] == "slpclient":
        print "client started"
        micro(TestTCPClient, (testAddress,))
        allmicro_run()
        print "client exited"
    elif sys.argv[1] == "server":
        print "server started"
        TestTCPServer(testAddress, socket.socket)
        print "server exited"
    elif sys.argv[1] == "slpserver":
        print "server started"
        micro(TestTCPServer,(testAddress,))
        allmicro_run()
        print "server exited"
    else:
        print "Usage:", sys.argv[0], "[client|server|slpclient|slpserver]"

    sys.exit(1)
else:
    micro(TestTCPServer, (testAddress,))
    micro(TestTCPClient, (testAddress,))
    allmicro_run()

    micro(TestMonkeyPatchUrllib, ("http://python.org/",))
    allmicro_run()

    TestMonkeyPatchUDP(testAddress)

    print "result: SUCCESS"
