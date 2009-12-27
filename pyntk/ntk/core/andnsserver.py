##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
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

from socket import AF_INET, SOCK_DGRAM

from ntk.lib import microsock
from ntk.lib.micro import microfunc, micro_block
from ntk.lib.xtime import swait

from ntk.lib.log import logger as logging

##
# This is just a simple standalone non-RPC UDP server.
# It is used to implement the ANDNS server socket.
##

def UDPServer(addr, requestHandler):
    """This function implement a simple UDP server """
    s=microsock.socket(AF_INET, SOCK_DGRAM)
    s.bind(addr)
    
    while True:
        try:
            message, address = s.recvfrom(1024, timeout=1000)
        except microsock.MicrosockTimeout:
            swait(500)
            continue
        requestHandler(s, address, message)
        swait(500)

class AndnsServer(object):
    
    def __init__(self, andns):
        self.andns = andns
        self.serv_sock = None

    def request_handler(self, socket, address, data):
        """ Keep and process the data from server socket and 
        reply """
        ## Botta e risposta
        logging.log(logging.ULTRADEBUG, "ANDNS Server: Received a DNS Query"
                                        " from " + str(address))
        request = message.from_wire(data)
        response = self.andns.process_binary(request)
        socket.sendto(response.to_wire(), address)
            
    @microfunc(True) 
    def daemon(self):
        """ Start the local DNS server """
        try:
            logging.log(logging.ULTRADEBUG, "ANDNS Server: starting daemon")
            UDPServer(('', 53), self.request_handler)
        except Exception, err:
            # restart
            logging.log(logging.ULTRADEBUG, str(err))
            micro_block()
            #self.daemon()
        
            
    def run(self):
        self.daemon()
        
    