import sys
import ntk.lib.microsock as microsock

class Sock:
    def __init__(self, net=None, me=None):
    	self._net = net
    	self._me  = me
    	self._socket = microsock

    def __getattr__(self, name):
	return getattr(self._socket, name)
