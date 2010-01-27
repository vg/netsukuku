##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
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

import os
import sys

from ntk.lib.log import logger as logging

def get_hostname():
    if sys.platform == 'linux2':
        if os.path.exists('/etc/hostname'):
            lines = []
            f = open('/etc/hostname')
            for l in f:
                if l[-1:] == '\n':
                    l = l[:-1]
                lines.append(l)
            f.close()
            for l in lines:
                if len(l) > 0 and l[0] != '#':
                    return l
            raise Exception('Hostname not defined.')
        else:
            raise Exception('Hostname not defined.')
    else:
        raise Exception('Your platform is not supported yet.')

def read_nodes(path):
    logging.debug('ANDNA: read_nodes(' + path + ').')
    snsd_nodes = []
    # first load the localcache using pickle
    if os.path.exists(path):
        file = open(path, 'r')
        for line in file:
            line.replace(" ", "")
            if line[-1:] == '\n':
                line = line[:-1]
            if line[0] != '#':
                snsd_nodes.append(line.split(':'))
                
    # now remove from snsd_nodes the name 
    # that are not into the local_cache
    return snsd_nodes

def parse_snsd_node(line, replace={}):
    logging.debug('ANDNA: parse_snsd_node' + str((line, replace)))
    def convert(line):       
        # line fields: 
        # APPEND = 0, HOSTNAME = 1, SNSD_RECORD = 2, SERV_KEY = 3,
        # PRIORITY = 4, WEIGHT = 5, SNSD_RECORD_PUBK = 6
        result = list(line)
        if result[0] == 'append':
            result[0] = True
        else:
            result[0] = False
        if result[2] == 'me' and replace.has_key('ME'):
            result[2] = replace['ME']
        result[3] = result[3]
        result[4] = int(result[4])
        result[5] = int(result[5])
        return result        
    if len(line) < 6:
        return (0, ())
    if len(line) == 6:
        # no public key path specified
        return (1, convert(line)+[None])
    if len(line) == 7:
        # public key of the SNSD Node specified
        return (1, convert(line))
    
def read_resolv(path):
    """ Returns a list of nameservers taken by the specified file, 
    replaced by localhost """
    logging.debug('ANDNA: read_resolv(' + path + ').')
    nameservers = []
    if not os.path.exists(path):
        open(path, 'w').close()        
    else:
        file = open(path, 'r')
        for line in file:
            line.replace(" ", "")
            if line[0] != '#' and 'nameserver' in line:
                nameservers.append(line.split('nameserver ')[1])
        os.rename(path, path+".original")
    file = open(path, 'w')
    file.writelines("nameserver 127.0.0.1\n")
    return nameservers

def restore_resolv(path):
    logging.debug('ANDNA: restore_resolv(' + path + ').')
    if os.path.exists(path):
        os.remove(path)
    if os.path.exists(path+".original"):
        os.rename(path+".original", path)
        
def is_ntk_name(hostname):
    if '.NTK.' in hostname:
        return 1
    else:
        return 0

def is_ip(address_string):
    i = 0
    for id in address_string.split('.'):
        i += 1
        try:
            int(id)
        except Exception, e:
            return False
        if i > 4:
            return False
    return True

def is_inaddr_arpa(address_string):
    if '.IN-ADDR.ARPA.' in address_string:
        return 1
    return 0
