ipv4 = 4
ipv6 = 6

def ip_bit(ipv):
    if ipv == ipv4:
	    return 32
    elif ipv == ipv6:
	    return 128

    raise Exception, str(ipv)+" is not a supported ip version"
