# Test suite for micro.py
# By Eriol

import sys
sys.path.append('..')

import logging
from wrap.sock import Sock
socket=Sock()
print socket.ManageSockets
import traceback

import stackless


class EchoServer:
    def __init__(self, host, port):
        self.host = host
        self.port = port

        stackless.tasklet(self.serve)()

    def serve(self):
        listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listen_socket.bind((self.host, self.port))
        listen_socket.listen(5)

        logging.info("Accepting connections on %s %s", self.host, self.port)
        try:
            while listen_socket.accepting:
                client_sock, client_addr = listen_socket.accept()
                stackless.tasklet(self.manage_connection)(client_sock, client_addr)
        except socket.error:
            traceback.print_exc()
	print "server closed"

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
    stackless.run()
