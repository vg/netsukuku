
from IN import SO_BINDTODEVICE
from socket import (inet_pton, inet_ntop, AF_INET, AF_INET6,SOL_SOCKET,
                    SO_BROADCAST)

ipv4 = 4
ipv6 = 6
ipfamily = {ipv4: AF_INET, ipv6: AF_INET6}
ipbit = {ipv4: 32, ipv6: 128}
familyver = {AF_INET: ipv4, AF_INET6: ipv6}

class Inet(object):

    def __init__(self, ip_version=ipv4, bits_per_level=8):
        self.ipv = ip_version
        self.bitslvl = bits_per_level

        if self.ipv == 6:
            raise NotImplementedError('IPV6 not supported yet!')

    def lvl_to_bits(self, lvl):
        return ipbit[self.ipv] - lvl*self.bitslvl

    def pip_to_ip(self, pip):
        ps = pip[::-1]
        return sum(ord(ps[i]) * 256**i for i in xrange(len(ps)))

    def ip_to_pip(self, ip):
        ver = self.ipv
        return ''.join([chr( (ip % 256**(i+1))/256**i ) for i in reversed(xrange(ipbit[ver]/8))])

    def pip_to_str(self, pip):
        return inet_ntop(ipfamily[self.ipv], pip)

    def str_to_pip(self, ipstr):
        return inet_pton(ipfamily[self.ipv], ipstr)

    def ip_to_str(self, ip):
        return self.pip_to_str(self.ip_to_pip(ip))

    def str_to_ip(self, ipstr):
        return self.pip_to_ip(self.str_to_pip(ipstr))

    def sk_bindtodevice(self, sck, devname):
        sck.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, devname)

    def sk_set_broadcast(self, sck, devname):
        if self.ipv == 4:
            sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

if __name__ == "__main__":
    ps = "1.2.3.4"

    I = Inet(ipv4, 2)

    assert I.lvl_to_bits(1) == 30
    pip = I.str_to_pip(ps)
    ip  = I.pip_to_ip(pip)
    PIP = I.ip_to_pip(ip)
    IP  = I.pip_to_ip(PIP)
    print str(ps)+" --> "+repr(pip)+" --> "+str(ip)+" --> "+repr(PIP)+" --> "+str(IP)
    assert PIP == pip
    assert IP == ip
    assert I.ip_to_str(ip) == ps
    print "all ok"
