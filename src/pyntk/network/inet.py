from socket import inet_pton, inet_ntop, AF_INET, AF_INET6, ntohl, htonl

ipv4 = 4
ipv6 = 6
ipfamily = {ipv4 : AF_INET, ipv6 : AF_INET6}
ipbit = {ipv4 : 32, ipv6 : 128}

def pip_to_ip(pip):
    ps = pip[::-1]
    return sum(ord(ps[i]) * 256**i for i in xrange(len(ps)))

def ip_to_pip(ip, ver):
    return ''.join([chr( (ip % 256**(i+1))/256**i ) for i in reversed(xrange(ipbit[ver]/8))])

def pip_to_str(pip, version=ipv4):
    return inet_ntop(ipfamily[version], pip)
def str_to_pip(ipstr, version=ipv4):
    return inet_pton(ipfamily[version], ipstr)

def ip_to_str(ip, version=ipv4):
    return pip_to_str(ip_to_pip(ip, version), version)
def str_to_ip(ipstr, version=ipv4):
    return pip_to_ip(str_to_pip(ipstr, version))

def sk_bindtodevice(sck, devname):
    sck.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, devname)

def sk_set_broadcast(sck, ipver, devname):
    if ipver == 4:
	    sck.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
    elif ipver == 6:
	    raise NotImplementedError, 'please, call a coder!'

    sck.bind(sck.getsockname())


if __name__ == "__main__":
	ps = "1.2.3.4"
	pv = ipv4

	pip = str_to_pip(ps, pv)
	ip  = pip_to_ip(pip)
	PIP = ip_to_pip(ip, pv)
	IP  = pip_to_ip(PIP)
	print str(ps)+" --> "+pip+" --> "+str(ip)+" --> "+PIP+" --> "+str(IP)
	assert PIP == pip
	assert IP == ip
	assert ip_to_str(ip, pv) == ps
	print "all ok"
