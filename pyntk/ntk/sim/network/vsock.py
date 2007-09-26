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

import os
import socket as stdsock
from errno import ENETUNREACH, EINVAL, ENOTSOCK, EPIPE

if "__all__" in stdsock.__dict__:
    __all__ = stdsock.__dict__
    for k, v in stdsock.__dict__.iteritems():
        if k in __all__:
            globals()[k] = v
else:
    for k, v in stdsock.__dict__.iteritems():
        if k.upper() == k:
            globals()[k] = v
    error = stdsock.error
    timeout = stdsock.timeout
    getaddrinfo = stdsock.getaddrinfo

from ntk.lib.micro import Channel
from ntk.network.inet import Inet, familyver
from ntk.sim.net import ESkt, ENotNeigh

class VirtualSocket(object):
    __slots__ = ['inet', 'net', 'me', 'addr_family', 'sck_type', 'sck',
                    'addr', 'node', 'broadcast', 'recvbuf']

    def __init__(self, address_family, socket_type, net, me):
        self.inet = Inet(ip_version=familyver[address_family])
        self.net=net # virtual Net instance
        self.me =me  # our Node instance

        self.addr_family = address_family
        self.sck_type = socket_type
        self.sck  = -1
        self.addr = -1
        self.node = None
        self.broadcast=False

        self.recvbuf = ''
    
    def accept(self):
        if self.sck_type == SOCK_DGRAM:
                raise error, (EINVAL, os.strerror(EINVAL))

        sck, node = self.me.accept()
        addr = self.inet.ip_to_str(node.ip)
        retsck = VirtualSocket(self.addr_family, self.sck_type, self.net, self.me)
        retsck.sck  = sck
        retsck.addr = addr
        retsck.node = node
        return retsck, (addr, 1)
   
    def bind(self, *args):pass

    def listen(self,n): pass

    def _get_net_node(self, addr):
        addr = self.inet.str_to_ip(addr)
        if not self.net.node_is_alive(addr):
                raise error, (ENETUNREACH, os.strerror(ENETUNREACH))

        return self.net.node_get(addr)

    def connect(self, (addr, port)):
        dst = self._get_net_node(addr)
        if self.sck_type != SOCK_DGRAM:
                try:
                        self.sck = self.me.connect(dst)
                except ENotNeigh:
                        print self.me.ip, dst.ip
                        raise error, (ENETUNREACH, os.strerror(ENETUNREACH))

        self.addr = addr
        self.node = dst
        return self.sck
    
    def connect_ex(self,(addr,port)):
        try:
                self.connect((addr,port))
        except error, (errno, str):
                return errno
        return 0

    def close(self):
        if self.sck_type != SOCK_DGRAM:
                self.me.close(self.node, self.sck)
        self.sck = -1
        self.addr= -1
        self.node= None

    def recv(self, buflen, flag=0):
        def _eat_data_from_buf(buflen):
                ret=self.recvbuf[:buflen]
                self.recvbuf=self.recvbuf[buflen:]
                return ret

        if len(self.recvbuf) >= buflen:
                return _eat_data_from_buf(buflen)

        try:
                if self.sck_type == SOCK_DGRAM: #UDP
                        if self.addr == -1:
                                raise ESkt

                        addr = None
                        while addr != self.addr:
                                msg, (addr, port) = self.recvfrom(buflen, flag)
                        self.recvbuf+=msg
                else: #TCP
                        self.recvbuf+=self.me.recv(self.sck)

                return _eat_data_from_buf(buflen)       

        except ESkt:
                raise error, (ENOTSOCK, os.strerror(ENOTSOCK))
   
    def recvfrom(self, buflen, flag=0):
        try:
                src, msg = self.me.recvfrom()
                return (msg, (self.inet.ip_to_str(src.ip), 0))
        except:
                raise error, (ENOTSOCK, os.strerror(ENOTSOCK))
    
    def sendall(self, data, flag=0):
        self.send(data, flag)
 
    def send(self, data, flag=0):
        try:
                if self.sck_type == SOCK_DGRAM: 
                        if self.addr == -1:
                                raise ESkt

                        return self.sendto(data, (self.addr, 0))
                else:
                        return self.me.send(self.node, self.sck, data)

        except ESkt:
                raise error, (EPIPE, os.strerror(EPIPE))
    
    def sendto(self, data, (addr, port)):
        dst = self._get_net_node(addr)
        try:
                if not self.broadcast:
                        return self.me.sendto(dst, data)
                else:
                        return self.me.sendtoall(data)
        except ENotNeigh:
                raise error, (ENETUNREACH, os.strerror(ENETUNREACH))

    def dup(self):
        retsck = VirtualSocket(None, self.sck_type, self.net, self.me)
        retsck.sck  = self.sck
        retsck.addr = self.addr
        retsck.node = self.node
        return retsck
    
    def fileno(self):
        return self.sck
    
    def getpeername(self):
        return (self.inet.ip_to_str(self.addr), 1)

    def getsockname(self):
        return (self.inet.ip_to_str(self.me.ip), 1)

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
