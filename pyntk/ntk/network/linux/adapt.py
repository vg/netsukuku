# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2008 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
import re
import subprocess

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.log import logger as logging
from ntk.network.interfaces import BaseNIC, BaseRoute

def file_write(path, data):
    ''' Writes `data' to the file pointed by `path' '''
    try:
        logging.info('\'' + data + '\'  -->  \'' + path + '\'')
    except:
        pass
    fout = open(path, 'w')
    try:
        fout.write(data)
    finally:
        fout.close()

class IPROUTECommandError(Exception):
    ''' A generic iproute exception '''

class IPTABLESCommandError(Exception):
    ''' A generic iproute exception '''

def iproute(args):
    ''' An iproute wrapper '''

    try:
        IPROUTE_PATH = settings.IPROUTE_PATH
    except AttributeError:
        IPROUTE_PATH = os.path.join('/', 'sbin', 'ip')

    if not os.path.isfile(IPROUTE_PATH):
        error_msg = ('Can not find %s.\n'
                     'Have you got iproute properly installed?')
        raise ImproperlyConfigured(error_msg % IPROUTE_PATH)

    args_list = args.split()
    cmd = [IPROUTE_PATH] + args_list
    logging.info(' '.join(cmd))
    proc = subprocess.Popen(cmd,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()

    if stderr_value:
        raise IPROUTECommandError(stderr_value)

    return stdout_value

def iptables(args):
    ''' An iptables wrapper '''

    try:
        IPTABLES_PATH = settings.IPTABLES_PATH
    except AttributeError:
        IPTABLES_PATH = os.path.join('/', 'sbin', 'iptables')

    if not os.path.isfile(IPTABLES_PATH):
        error_msg = ('Can not find %s.\n'
                     'Have you got iptables properly installed?')
        raise ImproperlyConfigured(error_msg % IPTABLES_PATH)

    args_list = args.split()
    cmd = [IPTABLES_PATH] + args_list
    logging.info(' '.join(cmd))
    proc = subprocess.Popen(cmd,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()

    if stderr_value:
        raise IPTABLESCommandError(stderr_value)

    return stdout_value

class NIC(BaseNIC):
    ''' Network Interface Controller handled using iproute '''

    def __init__(self, name):
        BaseNIC.__init__(self, name)
        self._mac = None

    def up(self):
        ''' Brings the interface up. '''
        iproute('link set %s up' % self.name)

    def down(self):
        ''' Brings the interface down. '''
        iproute('link set %s down' % self.name)

    def show(self):
        ''' Shows NIC information. '''
        return iproute('addr show %s' % self.name)

    def _flush(self):
        ''' Flushes all NIC addresses. '''
        iproute('addr flush dev %s' % self.name)

    def set_address(self, address):
        ''' Set NIC address.
        To remove NIC address simply put it to None or ''.

        @param address: Address of the interface.
        @type address: C{str}
        '''
        # We use only one address for NIC, so a flush is needed.
        if self.address:
            self._flush()
        if address is not None and address != '':
            iproute('addr add %s dev %s' % (address, self.name))

    def get_address(self):
        ''' Gets NIC address. '''
        if settings.IP_VERSION == 4:
            r = re.compile(r'''inet\s((?:\d{1,3}\.){3}\d{1,3})/''')
        matched_address = r.search(self.show())
        return matched_address.groups()[0] if matched_address else None

    def get_mac(self):
        ''' Gets MAC address. '''
        if not self._mac:
            r = re.compile('([a-fA-F0-9]{2}[:|\-]?){6}')
            s = self.show()
            matched_address = r.search(s)
            self._mac = s[matched_address.start():matched_address.end()] if matched_address else None
        return self._mac

    def get_is_active(self):
        ''' Returns True if NIC is active. '''
        r = re.compile(r'''<.*,(UP),.*>''')
        matched_active = r.search(self.show())
        return True if matched_active else False

    def filtering(self, *args, **kargs):
        ''' Enables or disables filtering. '''

        self._rp_filter(*args, **kargs)

    def _rp_filter(self, enable=True):
        ''' Enables or disables rp filtering. '''
        path = '/proc/sys/net/ipv%s' % settings.IP_VERSION
        path = os.path.join(path, 'conf', self.name, 'rp_filter')

        value = '1' if enable else '0'

        file_write(path, value)

mac_table = {}
# mac_table[w.upper()] is present (and has value n)
#   iff a routing table exists for packets coming from w
current_table = {}
# current_table[(None, v)] is present (and has value y)
#   iff a RULE destination = v → gateway = y exists in table
# current_table[(None, v)] is NOT present
#   iff no routes exist for destination = v → host/network is UNREACHABLE
# current_table[(w, v)] is present (and has value y)
#   iff a RULE source w, destination = v → gateway = y exists in table
# current_table[(w, v)] is present and has value None
#   iff a RULE source w, destination = v → host/network UNREACHABLE exists in table
# v is a ip/cidr string, eg '192.168.0.0/16';
# w is a MAC string, eg '6a:b8:1e:cf:1d:4f';
# y is a tuple of strings (ip, dev), eg ('192.12.1.1', 'eth1').


class Route(BaseRoute):
    ''' Managing routes using iproute '''

    @staticmethod
    def reset_routes():
        ''' We have no routes '''
        Route.flush()

    @staticmethod
    def default_route(ip, cidr, gateway, dev):
        ''' Maintains this default route for this destination. '''
        # ip and cidr are strings.
        # Eg: '192.168.0.0', '16'
        #
        # gateway and dev are strings.
        # Eg: '192.12.1.1', 'eth1'
        # If gateway is None then there are no routes (UNREACHABLE).
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.

        # When at level 0, that is cidr = lvl_to_bits(0), and ip = gateway,
        # the route has to be handled only as a neighbour.
        if cidr == 32 and ip == gateway: return

        ipcidr = ip + '/' + cidr
        if gateway:
            if (None, ipcidr) in current_table:
                if current_table[(None, ipcidr)] != (gateway, dev):
                    Route.change(ip, cidr, dev, gateway)
            else:
                Route.add(ip, cidr, dev, gateway)
            current_table[(None, ipcidr)] = (gateway, dev)
        else:
            if (None, ipcidr) in current_table:
                # retrieve old default gateway
                gateway, dev = current_table[(None, ipcidr)]
                cmd = Route.delete(ip, cidr, dev, gateway)
                del current_table[(None, ipcidr)]
            for k in current_table.keys():
                w, v = k
                if v == ipcidr:
                    # retrieve old gateway for prev_hop
                    if current_table[k] is None:
                        Route.delete_unreachable(ip, cidr, prev_hop=w)
                    else:
                        gateway, dev = current_table[k]
                        Route.delete(ip, cidr, dev, gateway, prev_hop=w)
                    del current_table[k]

    @staticmethod
    def prev_hop_route(ip, cidr, prev_hop, gateway, dev):
        ''' Maintains this route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''
        # ip, cidr and prev_hop are strings. prev_hop is a MAC address.
        # Eg: '192.168.0.0', '16', '6a:b8:1e:cf:1d:4f'
        #
        # gateway and dev are strings.
        # Eg: '192.12.1.1', 'eth1'
        # If gateway is None then there are no routes (UNREACHABLE).
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.

        # When at level 0, that is cidr = lvl_to_bits(0), and ip = gateway,
        # the route has to be handled only as a neighbour.
        if cidr == 32 and ip == gateway: return

        ipcidr = ip + '/' + cidr
        if gateway:
            if (prev_hop, ipcidr) in current_table:
                if current_table[(prev_hop, ipcidr)] != (gateway, dev):
                    Route.change(ip, cidr, dev, gateway, prev_hop=prev_hop)
            else:
                Route.add(ip, cidr, dev, gateway, prev_hop=prev_hop)
            current_table[(prev_hop, ipcidr)] = (gateway, dev)
        else:
            if (prev_hop, ipcidr) in current_table:
                if current_table[(prev_hop, ipcidr)] is not None:
                    Route.unreachable(ip, cidr, prev_hop=prev_hop)
            else:
                Route.unreachable(ip, cidr, prev_hop=prev_hop)
            current_table[(prev_hop, ipcidr)] = None

    @staticmethod
    def prev_hop_route_default(ip, cidr, prev_hop):
        ''' Use default route for this destination when prev_hop is the
            gateway from which we received the packet.
        '''
        # ip, cidr and prev_hop are strings. prev_hop is a MAC address.
        # Eg: '192.168.0.0', '16', '6a:b8:1e:cf:1d:4f'
        #
        # The implementation has to know by itself if it needs to
        # add, change or remove any route.

        ipcidr = ip + '/' + cidr
        if (prev_hop, ipcidr) in current_table:
            if current_table[(prev_hop, ipcidr)] is None:
                Route.delete_unreachable(ip, cidr, prev_hop=prev_hop)
            else:
                gateway, dev = current_table[(prev_hop, ipcidr)]
                Route.delete(ip, cidr, dev, gateway, prev_hop=prev_hop)
            del current_table[(prev_hop, ipcidr)]

    @staticmethod
    def _table_for_macaddr(macaddr):
        ''' Makes sure that a routing table exists for packets coming from macaddr.
            Returns the number of that table.
        '''
        if macaddr.upper() not in mac_table:
            # Find first integer not used, starting from 26
            # TODO: is the number ok?
            idn = 26
            while idn in mac_table.values():
                idn += 1
            # Add a routing table for mac prev_hop with number idn
            args = '-t mangle -A PREROUTING -m mac --mac-source ' + macaddr + ' -j MARK --set-mark ' + str(idn)
            iptables(args)
            args = 'rule add fwmark ' + str(idn) + ' table ' + str(idn)
            iproute(args)
            # Add in mac_table
            mac_table[macaddr.upper()] = idn
        else:
            idn = mac_table[macaddr.upper()]
        return idn

    @staticmethod
    def _table_for_macaddr_remove_all():
        ''' removes all routing table created for packets coming from any macaddr
        '''
        all_macs = mac_table.keys()
        for k in all_macs:
            Route._table_for_macaddr_remove(k)

    @staticmethod
    def _table_for_macaddr_remove(macaddr):
        ''' Makes sure that a routing table doesn't exist anymore for packets coming from macaddr
        '''
        if macaddr.upper() in mac_table:
            idn = mac_table[macaddr.upper()]
            # Remove the association to routing table with number idn for mac prev_hop
            args = '-t mangle -D PREROUTING -m mac --mac-source ' + macaddr + ' -j MARK --set-mark ' + str(idn)
            iptables(args)
            args = 'rule del fwmark ' + str(idn) + ' table ' + str(idn)
            iproute(args)
            # Remove from mac_table
            del mac_table[macaddr.upper()]

    @staticmethod
    def _modify_routes_cmd(command, ip, cidr, dev, gateway, table=None):
        ''' Returns proper iproute command arguments to add/change/delete routes
        '''
        cmd = 'route %s %s/%s' % (command, ip, cidr)

        if table is not None:
            cmd += ' table %s' % table
        if dev is not None:
            cmd += ' dev %s' % dev
        if gateway is not None:
            cmd += ' via %s' % gateway

        cmd += ' protocol ntk'

        return cmd

    @staticmethod
    def _modify_neighbour_cmd(command, ip, dev):
        ''' Returns proper iproute command arguments to add/change/delete a neighbour
        '''
        cmd = 'route %s %s' % (command, ip)

        if dev is not None:
            cmd += ' dev %s' % dev

        cmd += ' protocol ntk'

        return cmd

    @staticmethod
    def add(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Adds a new route with corresponding properties. '''
        # When at level 0, that is cidr = lvl_to_bits(0), this method
        # might be called with ip = gateway. In this case the command
        # below makes no sense and would result in a error.
        if cidr == 32 and ip == gateway: return
        if prev_hop is None:
            cmd = Route._modify_routes_cmd('add', ip, cidr, dev, gateway)
            iproute(cmd)
        else:
            idn = Route._table_for_macaddr(prev_hop)
            cmd = Route._modify_routes_cmd('add', ip, cidr, dev, gateway, table=idn)
            iproute(cmd)

    @staticmethod
    def change(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Edits the route with corresponding properties. '''
        # When at level 0, that is cidr = lvl_to_bits(0), this method
        # might be called with ip = gateway. In this case the command
        # below makes no sense and would result in a error.
        if cidr == 32 and ip == gateway: return
        if prev_hop is None:
            cmd = Route._modify_routes_cmd('change', ip, cidr, dev, gateway)
            iproute(cmd)
        else:
            idn = Route._table_for_macaddr(prev_hop)
            cmd = Route._modify_routes_cmd('change', ip, cidr, dev, gateway, table=idn)
            iproute(cmd)

    @staticmethod
    def delete(ip, cidr, dev=None, gateway=None, prev_hop=None):
        ''' Removes the route with corresponding properties. '''
        # When at level 0, that is cidr = lvl_to_bits(0), this method
        # might be called with ip = gateway. In this case the command
        # below makes no sense and would result in a error.
        if cidr == 32 and ip == gateway: return
        if prev_hop is None:
            cmd = Route._modify_routes_cmd('del', ip, cidr, dev, gateway)
            iproute(cmd)
        else:
            idn = Route._table_for_macaddr(prev_hop)
            cmd = Route._modify_routes_cmd('del', ip, cidr, dev, gateway, table=idn)
            iproute(cmd)

    @staticmethod
    def unreachable(ip, cidr, prev_hop):
        ''' Claim unreachable the route with corresponding properties. '''
        idn = Route._table_for_macaddr(prev_hop)
        cmd = 'route add table ' + str(idn) + ' unreachable ' + str(ip) + '/' + str(cidr)
        iproute(cmd)

    @staticmethod
    def delete_unreachable(ip, cidr, prev_hop):
        ''' Stop claiming unreachable the route with corresponding properties. '''
        idn = Route._table_for_macaddr(prev_hop)
        cmd = 'route del table ' + str(idn) + ' unreachable ' + str(ip) + '/' + str(cidr)
        iproute(cmd)

    @staticmethod
    def add_neigh(ip, dev=None):
        ''' Adds a new neighbour with corresponding properties. '''
        cmd = Route._modify_neighbour_cmd('add', ip, dev)
        iproute(cmd)

    @staticmethod
    def change_neigh(ip, dev=None):
        ''' Edits the neighbour with corresponding properties. '''
        # If a neighbour previously reached via eth0, now has a better link
        # in eth1, it makes sense to use this command.
        cmd = Route._modify_neighbour_cmd('change', ip, dev)
        iproute(cmd)

    @staticmethod
    def delete_neigh(ip, dev=None):
        ''' Removes the neighbour with corresponding properties. '''
        cmd = Route._modify_neighbour_cmd('del', ip, None)
        iproute(cmd)

    @staticmethod
    def flush():
        ''' Flushes the routing table '''
        global current_table
        global mac_table
        try:
            iproute('route flush table main')
        except:
            # It could result in "Nothing to flush". It's ok.
            pass
        for idn in mac_table.values():
            try:
                iproute('route flush table ' + str(idn))
            except:
                # It could result in "Nothing to flush". It's ok.
                pass
        Route._table_for_macaddr_remove_all()
        current_table = {}
        mac_table = {}

    @staticmethod
    def flush_cache():
        ''' Flushes cache '''
        iproute('route flush cache')

    @staticmethod
    def ip_forward(enable=True):
        ''' Enables/disables ip forwarding. '''
        path = '/proc/sys/net/ipv%s' % settings.IP_VERSION

        if settings.IP_VERSION == 4:
            path = os.path.join(path, 'ip_forward')
        elif settings.IP_VERSION == 6:
            # Enable forwarding for all interfaces
            path = os.path.join(path, 'conf/all/forwarding')

        value = '1' if enable else '0'

        file_write(path, value)
