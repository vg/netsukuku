##
# This file is part of Netsukuku
# (c) Copyright 2007 Alberto Santini <alberto@unix-monk.com>
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

import commands
import logging

class Route:
  """ this class is used to manage routes """

  # we're using netsukuku, ain't we?
  protocol = "ntk"

  def __init__(self, ip_version = 4):
    """ ip_version: tells if we're using IPv4 or IPv6 """

    self.ip_version = ip_version

  def _exec_ipr(self, ipr_str):
    """ ipr_str: ip route string """

     # execute ip route
    status, output = commands.getstatusoutput(ipr_str)

    # check if everything went good.. if not, log!
    if(status != 0):
	    logger.error(output)

  def _ip_route_gen_str(self, destination_ip = None, destination_bit = None,
		  protocol = "ntk", table = None, net_device = None, gateway = None):
    """ destination_ip: destination node's ip address
        destination_bit: destination node ip's bitmask
        protocol: the protocol we're using
        table: kernel's routing table wehre the route [is | has to be] stored
        net_device: nic we use to get to destination
        gateway: the gateway to use to reach the destination node
        generate 'ip route'-compatible strings (with a starting space)
    """

    # ip route string
    ipr_str = ""
    
    # if an ip is provided, a bitmask has to, also. in this case, use them
    if(destination_ip != None):
      if destionation_bit == None:
        logger.error("Invalid ip provided (missing bitmask)")
      else:
        ipr_str += " to " + str(destination_ip) + "/" + str(destination_bit)
    
    # if protocol is provided use it
    if(protocol != None): ipr_str += " protocol " + str(protocol)

    # if table is provided use it
    if(table != None): ipr_str += " table " + str(table)
    
    # if net_device is provided use it
    if(net_device != None): ipr_str += " dev " + str(net_device)
    
    # if gateway is provided use it
    if(gateway != None): ipr_str += " via " + str(gateway)

  def _ip_route_add(self, destination_ip, destination_bit, table, net_device, gateway):
    """ add a route in kernel routing table, via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route add" + _ip_route_gen_str(destination_ip, destination_bit, Route.protocol, table, net_device, gateway)
    
    # execute ip route
    _exec_ipr(ipr_str)

  def _ip_route_delete(self, destination_ip, destination_bit, table = None, net_device = None, gateway = None):
    """ delete a route from kernel routing table, via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route delete" + _ip_route_gen_str(destination_ip, destination_bit, Route.protocol, table, net_device, gateway)

    # execute ip route
    _exec_ipr(ipr_str)

  def _ip_route_flush(self, destination_ip = None, destination_bit = None, table = None, net_device = None, gateway = None):
    """ flush some routes from kernel routing table via 'ip route' """

    # check wether we have at least one valid parameter
    if((destination_ip == None) and (table == None) and (net_device == None) and (destination == None)):
      logger.error("Cannot flush without valid criteria")

    # generate the ip route string
    ipr_str = "ip route flush" + _ip_route_gen_str(destination_ip, destination_bit, Route.protocol, table, net_device, gateway)

    # execute ip route
    _exec_ipr(ipr_str)

  def _ip_route_change(self, destination_ip, destination_bit, table = None, net_device = None, gateway = None):
    """ change a route in kernel routing table via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route change" + _ip_route_gen_str(destination_ip, destination_bit, Route.protocol, table, net_device, gateway)
    
    # execute ip route
    _exec_ipr(ipr_str)

  def route_add(self, destination_ip, destination_bit, table, net_device, gateway):
    """ add a route in kernel routing table """
    _ip_route_add(destination_ip, destination_bit, table, net_device, gateway)

  def route_delete(self, destination_ip, destination_bit, table = None, net_device = None, gateway = None):
    """ delete a route from kernel routing table """
    _ip_route_delete(destination_ip, destination_bit, table, net_device, gateway)

  def route_flush(self, destination_ip = None, destination_bit = None, table = None, net_device = None, gateway = None):
    """ flush some routes from kernel routing table """
    _ip_route_flush(self, destination_ip, destination_bit, table, net_device, gateway)

  def route_change(self, destination_ip, destination_bit, table = None, net_device = None, gateway = None):
    """ change a route in kernel routing table """
    _ip_route_change(destination_ip, destination_bit, table, net_device, gateway)

  def route_rp_filter_enable(net_device):
    """ enable rp filtering on net_device """
    _route_rp_filter(net_device, True)

  def route_rp_filter_disable(net_device):
    """ disable rp filtering on net_device """
    _route_rp_filter(net_device, False)
    
  def route_ip_forward_enable(self):
    """ enable ip forwarding """
    _route_ip_forward(True)

  def route_ip_forward_disable(self):
    """ disable ip forwarding """
    _route_ip_forward(False)

  def route_flush_cache(self):
    """ flush kernel route cache """
    _proc_route_flush_cache()

  def _route_rp_filter(net_device, enable):
    """ enable/disable rp filtering """
    _proc_route_rp_filter(net_device, enable)
    
  def _route_ip_forward(enable):
    """ enable/disable ip forwarding """
    _proc_route_ip_forward(enable)

  def _proc_route_rp_filter(net_device, enable):
    """ enable/disable rp filtering via '/proc' """

    # where to write in /proc
    proc_path = "/proc/sys/net/ipv" + str(self.ip_version) + "/conf/" + str(net_device) + "/rp_filter"
    
    # what to write in /proc
    if(enable):
      proc_value = "1"
    else:
    	proc_value = "0"

    # write in /proc
    _proc_write(proc_path, proc_value)
  
  def _proc_ip_forward(enable):
    """ enable/disable ip forwarding via '/proc' """
    
    # where to write in /proc
    proc_path = "/proc/sys/net/ipv" + str(self.ip_version)
    if(self.ip_version == 4):
      proc_path += "/ip_forward"
    else:
      proc_path += "/conf/all/forwarding"
  
  	# what to write in /proc
      if enable:
  	  proc_value = "1"
      else:
  	  proc_value = "0"
  
    # write in proc
    _proc_write(proc_path, proc_value)

  def _proc_flush_cache(self):
    """ flush kernel route cache via '/proc' """
    
    # where to write in /proc
    proc_path = "/proc/sys/net/ipv" + str(self.ip_version) + "/route/flush"
    
    # what to write in /proc
    proc_value = "-1"
    
    # write in /proc
    _proc_write(proc_path, proc_value)

  def _proc_write(proc_path, proc_value):
    """ write proc_value into proc_path """
    
    try:
      # open proc_path
      file_handler = open(proc_path, "w")
      # write
      file_handler.write(str(proc_value))
      # close proc_file
      file_handler.close()
    except IOError, err:
      logger.error(err)
