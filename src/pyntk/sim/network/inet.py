#!/usr/bin/env python
#
#  This file is part of Netsukuku
#  (c) Copyright 2007 Andrea Milazzo aka Mancausoft <andreamilazzo@mancausoft.org>
# 
#  This source code is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as published 
#  by the Free Software Foundation; either version 2 of the License,
#  or (at your option) any later version.
# 
#  This source code is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  Please refer to the GNU Public License for more details.
# 
#  You should have received a copy of the GNU Public License along with
#  this source code; if not, write to:
#  Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

import SocketServer
#import ntksim
import sys
import os
import socket
from errno import ENETUNREACH, EINVAL

sys.path+=['..']
from net import ESkt, ENotNeigh

sys.path+=['../../']
from lib.inet import ip_to_str, str_to_ip
from lib.micro import Channel

class VirtualSocket:
    
    def __init__(self, address_family, socket_type, net, me):
        self.net=net # virtual Net instance
	self.me =me  # our Node instance

	self.sck  = -1
	self.sck_type = socket_type
	self.addr = -1
	self.node = None
    
    def accept(self):
	if self.sck_type == socket.SOCK_DGRAM:
		raise socket.error, (EINVAL, os.strerror(EINVAL))

        sck, node = self.me.accept()
	addr = node.ip
	retsck = VirtualSocket(None, self.sck_type, self.net, self.me)
	retsck.sck  = sck
	retsck.addr = addr
	retsck.node = node
        return retsck, (ip_to_str(addr), 1)
   
    def bind(self, *args):pass

    def listen(self,n): pass

    def _get_net_node(self, addr):
	addr = str_to_ip(addr)
	if not self.net.node_is_alive(addr):
		raise socket.error, (ENETUNREACH, os.strerror(ENETUNREACH))

	return self.net.node_get(addr)

    def connect(self, (addr, port)):
	if self.sck_type == socket.SOCK_DGRAM:
		return 0

	dst = self._get_net_node(addr)
	try:
		self.sck = self.me.connect(dst)
	except ENotNeigh:
		raise socket.error, (ENETUNREACH, os.strerror(ENETUNREACH))

        self.addr = addr
	self.node   = node
        return self.sck
    
    def connect_ex(self,(addr,port)):
	try:
		self.connect((addr,port))
	except socket.error, (errno, str):
		return errno
	return 0

    def close(self):
        self.me.close(self.sck)
        self.sck = -1
	self.addr= -1
	self.node= None

    def recv(self, buflen, flag=0): 
	try:
        	return self.me.recv(self.sck)
	except ESkt:
		raise socket.error, (ENOTSOCK, os.strerror(ENOTSOCK))
   
    def recvfrom(self, buflen, flag=0):
	try:
        	src, msg = self.me.recvfrom(self.sck)
		return (msg, ip_to_str(src.ip))
	except:
		raise socket.error, (ENOTSOCK, os.strerror(ENOTSOCK))
        return self.recv(self, buflen, flag)
    
    def sendall(self, data, flag=0):
        self.send(data, flag)
 
    def send(self, data, flag=0):
	try:
        	return self.me.send(self.node, self.sck, data)
	except ESkt:
		raise socket.error, (EPIPE, os.strerror(EPIPE))
    
    def sendto(self, data, (addr, port)):
	dst = self._get_net_node(addr)
	try:
		if not self.broadcast:
        		return self.me.sendto(dat, data)
		else:
        		return self.me.sendtoall(data)
	except ENotNeigh:
		raise socket.error, (ENETUNREACH, os.strerror(ENETUNREACH))

    def dup(self):
	retsck = VirtualSocket(None, self.sck_type, self.net, self.me)
	retsck.sck  = self.sck
	retsck.addr = self.addr
	retsck.node = self.node
	return retsck
    
    def fileno(self):
	return self.sck
    
    def getpeername(self):
	return (ip_to_str(self.addr), 1)

    def getsockname(self):
	return (ip_to_str(self.me.ip), 1)

    def getsockopt(self, level, optname, buflen=0):
        pass

    def gettimeout(self):
	return None

    def makefile(self,mode=0,bufsize=0):
        pass

    def setblocking(self, flag):
        pass

    def setsockopt(self, level, optname, value):
	if level == SOL_SOCKET and optname == SO_BROADCAST:
		self.broadcast=value
	if level == IPPROTO_IPV6 and IPV6_JOIN_GROUP:
		self.broadcast=1
    
    def settimeout(self, flag):
        pass

    def shutdown(self, how):
        self.close()

class TCPVirtualServer(SocketServer.TCPServer):

    def __init__(self, server_address, RequestHandlerClass, net, me):
        BaseServer.__init__(self, server_address, RequestHandlerClass) 
        self.socket = VirtualSocket(self.address_family,
                                    self.socket_type, net, me)
