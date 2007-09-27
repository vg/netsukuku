import sys
import socket
import ntk.sim.network.vsock as vsock

class Sock:
    def __init__(self, net, me, del_mods=[]):
        self._net = net
        self._me  = me
        self._socket = vsock
        
        if type(del_mods) == str:
                del_mods=[del_mods]

        del_mods.append('socket')
        for m in del_mods:
                try: del sys.modules[m]
                except: pass
        #for m in sys.modules.keys():
        #        if 'socket' in m:
        #                del sys.modules[m]
        sys.modules['socket']=self

    def __getattr__(self, name):
        return getattr(self._socket, name)

    def socket(self, family=socket.AF_INET, type=socket.SOCK_STREAM, proto=0):
        return self._socket.VirtualSocket(family, type, self._net, self._me)
