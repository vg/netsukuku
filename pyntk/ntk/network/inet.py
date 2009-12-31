##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
#
# Refactored by Daniele Tricoli on 11/2008.
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
#
# (c) Copyright 2009 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
#
# This source code is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published 
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This source code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# Please refer to the GNU Public License for more details.
#
# You should have received a copy of the GNU Public License along with
# this source code; if not, write to:
# Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
#
# Various network utility function
#


from ntk.lib.log import logger as logging
import socket
import sys

from ntk.config import settings


ipv4 = 4
ipv6 = 6
ipfamily = {ipv4: socket.AF_INET, ipv6: socket.AF_INET6}
ipbit = {ipv4: 32, ipv6: 128}
familyver = {socket.AF_INET: ipv4, socket.AF_INET6: ipv6}

# Compatibility functions that will work only with IPV4
# For now this is not a problem because IPV6 is currently disabled.
def _inet_ntop(family, address):
    if family == socket.AF_INET:
        return socket.inet_ntoa(address)

def _inet_pton(family, address):
    if family == socket.AF_INET:
        return socket.inet_aton(address)

try:
    socket.inet_pton
except AttributeError:
    socket.inet_pton = _inet_pton
    socket.inet_ntop = _inet_ntop

def lvl_to_bits(lvl):
    ''' Returns bits corresponding to `lvl' '''
    return ipbit[settings.IP_VERSION] - lvl*settings.BITS_PER_LEVEL

def pip_to_ip(pip):
    ps = pip[::-1]
    return sum(ord(ps[i]) * 256**i for i in xrange(len(ps)))

def ip_to_pip(ip):
    ver = settings.IP_VERSION
    return ''.join([chr( (ip % 256**(i+1))/256**i ) for i in reversed(xrange(ipbit[ver]/8))])

def pip_to_str(pip):
    return socket.inet_ntop(ipfamily[settings.IP_VERSION], pip)

def str_to_pip(ipstr):
    return socket.inet_pton(ipfamily[settings.IP_VERSION], ipstr)

def ip_to_str(ip):
    return pip_to_str(ip_to_pip(ip))

def str_to_ip(ipstr):
    return pip_to_ip(str_to_pip(ipstr))

# Invalid IPs are:
# 192.168.* that are private network address
# 224.* to 255.*, the multicast and badclass address range
# 127.* the loopback address
# 0.* of course
valid_id_list_for_3 = range(1, 127)
valid_id_list_for_3 += range(128, 224)
valid_id_list_for_2_after_192 = range(0, 168)
valid_id_list_for_2_after_192 += range(169, 256)
def valid_ids(lvl, nip):
    '''Returns the list of valid IDs for level lvl, given that IDs for upper
    levels are already choosen in nip.
    E.g. what are valid IDs for level 2 when level 3 is 192?
    valid_ids(2, [0, 0, 0, 192])
    Elements of nip for levels <= lvl is not relevant'''

    #first thing is to see what is the IPVERSION
    if settings.IP_VERSION == ipv6:
        return xrange(256)
    else:
        if lvl == 3:
            return valid_id_list_for_3
        elif lvl == 2:
            if nip[3] == 192:
                return valid_id_list_for_2_after_192
            else:
                return xrange(256)
        else:
            return xrange(256)

def sk_bindtodevice(sck, devname):
    if sys.platform == 'linux2':
        from IN import SO_BINDTODEVICE
        sck.setsockopt(socket.SOL_SOCKET, SO_BINDTODEVICE, devname)
    else:
        m = 'SO_BINDTODEVICE is not supported by your platform.'
        raise NotImplementedError(m)

def sk_set_broadcast(sck):
    if settings.IP_VERSION == 4:
        sck.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
