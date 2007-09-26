class Sock:
    def __init__(self, net=None, me=None):
    	self._net = net
    	self._me  = me
    	import microsock
    	self._socket = microsock
	import sys
	sys.modules['socket']=self

    def __getattr__(self, name):
	return getattr(self._socket, name)
