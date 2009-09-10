##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
# A testing stackless ECHO server
#

import sys
sys.path.append('..')

import logging
from ntk.wrap.sock import Sock
socket=Sock()

import traceback

import stackless

class EchoServer(object):
    def __init__(self, host, port):
        self.host = host
        self.port = port

        stackless.tasklet(self.serve)()

    def serve(self):
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listen_socket.bind((self.host, self.port))
        listen_socket.listen(5)

        logging.info('Accepting connections on %s %s', self.host, self.port)
        try:
            while listen_socket.accepting:
                client_sock, client_addr = listen_socket.accept()
                stackless.tasklet(self.manage_connection)(client_sock, client_addr)
        except socket.error:
            traceback.print_exc()

    def manage_connection(self, client_sock, client_addr):
        data = ''
        try:
                while client_sock.connected:
                    data += client_sock.recv(1024)
                    if data == '':
                        break
                    client_sock.send(data)
                    data = ''
        except:
                traceback.print_exc()
        print "connection closed"
        raise SystemExit

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG,
                        format='%(asctime)s %(levelname)s %(message)s')
    server = EchoServer('127.0.0.1', 1234)
    try:
        stackless.run()
    except KeyboardInterrupt:
        logging.info('Stopping ECHO server')
        sys.exit()
