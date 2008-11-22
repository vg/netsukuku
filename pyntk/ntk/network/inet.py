
from IN import SO_BINDTODEVICE
from socket import (inet_pton, inet_ntop, AF_INET, AF_INET6, SOL_SOCKET,
                    SO_BROADCAST)

from ntk.config import settings

ipv4 = 4
ipv6 = 6
ipfamily = {ipv4: AF_INET, ipv6: AF_INET6}
ipbit = {ipv4: 32, ipv6: 128}
familyver = {AF_INET: ipv4, AF_INET6: ipv6}


def lvl_to_bits(lvl):
    return ipbit[settings.IP_VERSION] - lvl*settings.BITS_PER_LEVEL

def pip_to_ip(pip):
    ps = pip[::-1]
    return sum(ord(ps[i]) * 256**i for i in xrange(len(ps)))

def ip_to_pip(ip):
    ver = settings.IP_VERSION
    return ''.join([chr( (ip % 256**(i+1))/256**i ) for i in reversed(xrange(ipbit[ver]/8))])

def pip_to_str(pip):
    return inet_ntop(ipfamily[settings.IP_VERSION], pip)

def str_to_pip(ipstr):
    return inet_pton(ipfamily[settings.IP_VERSION], ipstr)

def ip_to_str(ip):
    return pip_to_str(ip_to_pip(ip))

def str_to_ip(ipstr):
    return pip_to_ip(str_to_pip(ipstr))

def sk_bindtodevice(sck, devname):
    sck.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, devname)

def sk_set_broadcast(sck):
    if settings.IP_VERSION == 4:
        sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

if __name__ == "__main__":
    ps = "1.2.3.4"

    assert lvl_to_bits(1) == 24
    pip = str_to_pip(ps)
    ip  = pip_to_ip(pip)
    PIP = ip_to_pip(ip)
    IP  = pip_to_ip(PIP)
    print str(ps)+" --> "+repr(pip)+" --> "+str(ip)+" --> "+repr(PIP)+" --> "+str(IP)
    assert PIP == pip
    assert IP == ip
    assert ip_to_str(ip) == ps
    print "all ok"
