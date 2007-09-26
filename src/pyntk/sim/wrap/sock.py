import socket
import sys
sys.path.append('..')
import network.vsock as vsock

class Sock:
    def __init__(self, net, me):
    	self._net = net
    	self._me  = me
    	self._socket = vsock
	try: del sys.modules['socket']
	except: pass
	sys.modules['socket']=self

    def __getattr__(self, name):
	return getattr(self._socket, name)

    def socket(self, family=socket.AF_INET, type=socket.SOCK_STREAM, proto=0):
        return self._socket.VirtualSocket(family, type, self._net, self._me)
