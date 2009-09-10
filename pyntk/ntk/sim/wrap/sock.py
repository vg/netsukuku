import sys
import socket
import ntk.sim.network.vsock as vsock

class Sock:
    def __init__(self, net, me):
        self._net = net
        self._me  = me
        self._socket = vsock

    def __getattr__(self, name):
        return getattr(self._socket, name)

    def socket(self, family=socket.AF_INET, type=socket.SOCK_STREAM, proto=0):
        return self._socket.VirtualSocket(family, type, self._net, self._me)
