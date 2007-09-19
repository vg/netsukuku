from socket import inet_pton, inet_ntop, AF_INET, AF_INET6, ntohl, htonl, \
		   SOL_SOCKET, SO_BROADCAST
from IN import SO_BINDTODEVICE

ipv4 = 4
ipv6 = 6
ipfamily = {ipv4 : AF_INET, ipv6 : AF_INET6}
ipbit = {ipv4 : 32, ipv6 : 128}
familyver = { AF_INET : ipv4, AF_INET6 : ipv6}
 
class Inet:
  
    def __init__(S, ip_version=ipv4, bits_per_level=8):
        S.ipv = ip_version

	S.bitslvl = bits_per_level
	S.gsize   = 2**(S.bitslvl)

    def lvl_to_bits(S, lvl):
	return ipbit[S.ipv]-lvl*S.bitslvl

    def pip_to_ip(S, pip):
        ps = pip[::-1]
        return sum(ord(ps[i]) * S.gsize**i for i in xrange(len(ps)))
    
    def ip_to_pip(S, ip):
	ver=S.ipv
        return ''.join([chr( (ip % S.gsize**(i+1))/S.gsize**i ) for i in reversed(xrange(ipbit[ver]/8))])
    
    def pip_to_str(S, pip):
        return inet_ntop(ipfamily[S.ipv], pip)
    def str_to_pip(S, ipstr):
        return inet_pton(ipfamily[S.ipv], ipstr)
    
    def ip_to_str(S, ip):
        return S.pip_to_str(S.ip_to_pip(ip))
    def str_to_ip(S, ipstr):
        return S.pip_to_ip(S.str_to_pip(ipstr))
    
    def sk_bindtodevice(S, sck, devname):
        sck.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, devname)
    
    def sk_set_broadcast(S, sck, devname):
        if S.ipv == 4:
    	    sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
        elif S.ipv == 6:
    	    raise NotImplementedError, 'please, call a coder!'
    
    
if __name__ == "__main__":
	ps = "1.2.3.4"

	I = Inet(ipv4, 8)
	
	assert I.lvl_to_bits(1) == 24
	pip = I.str_to_pip(ps)
	ip  = I.pip_to_ip(pip)
	PIP = I.ip_to_pip(ip)
	IP  = I.pip_to_ip(PIP)
	print str(ps)+" --> "+pip+" --> "+str(ip)+" --> "+PIP+" --> "+str(IP)
	assert PIP == pip
	assert IP == ip
	assert I.ip_to_str(ip) == ps
	print "all ok"
