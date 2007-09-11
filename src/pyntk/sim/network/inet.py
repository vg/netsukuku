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
sys.path+=['..', '../../']
from lib.micro import Channel

class VirtualSocket:
    
    def __init__(self, address_family, socket_type, net):
        self.net=net # virtual Net instance

	self.sck=None
    
    def accept(self, addr=None, sck=None):
        self.sck=sck
        ntksim.accept(self)
        return self.sck
   
    def bind(self, addr):
        self.addr = addr

    def close(self):
        self.sck=None

    def connect(self, addr, t=None): #TODO: connect error 
        self.sck = randint()
        self.chan = Channel() 
        self.t = t
        self.r_addr = addr
        self.net.connect(self)
        return self.sck
    
    def connect_ex(self,addr):
        self.connect(addr)
        return self.sck

    def dup(self):
        pass
    
    def fileno(self):
        pass
    
    def getpeername(self):
        pass

    def getsockname(self):
        pass

    def getsockopt(self, level, optname, buflen=0):
        pass

    def gettimeout(self):
        pass

    def listen(self,n):
        pass

    def makefile(self,mode=0,bufsize=0):
        pass
    
    def recv(self, buflen, flag=0): 
        if not self.sck: return None
        return self.chan.rcv()
   
    def recvfrom(self, buflen, flag=0):
        return self.recv(self, buflen, flag)
    
    def sendall(self, data, flag=0):
        self.send(data, flag)
        pass
 
    def send(self, data, flag=0):
        ntksim.send(self, data, flag)
        pass
    
    def sendto(self, data,falg=0):
        self.send(data, flag)
        pass

    def setblocking(self, flag):
        pass

    def setsockopt(self, level, optname, value):
        pass
    
    def settimeout(self, flag):
        pass

    def shutdown(self, how):
        pass

class TCPVirtualServer(SocketServer.TCPServer):

    def __init__(self, server_address, RequestHandlerClass):
        BaseServer.__init__(self, server_address, RequestHandlerClass) 
        self.socket = VirtualSocket(self.address_family,
                                    self.socket_type)

