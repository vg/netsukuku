import commands
import logging

class Route:
  """ this class is used to manage routes """
  def __init__(self):
    """ init """

    # we're using netsukuku, ain't we?
    self.protocol = "netsukuku"

  def exec_ipr(self, ipr_str):
    """ ipr_str: ip route string """

     # execute ip route
     status, output = commands.getstatusoutput(ipr_str)

     # check if everything went good.. if not, log!
     if(status != 0):
        logger.error(output)

  def ip_route_gen_str(self, destination_ip = None, destination_bit = None , protocol = self.protocol, table = None, net_device = None, gateway = None):
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
      if(destionation_bit == None)):
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

  def ip_route_add(self, destination_ip, destination_bit, protocol = self.protocol, table, net_device, gateway):
    """ add a route in kernel routing table, via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route add" + ip_route_gen_str(destination_ip, destination_bit, protocol, table, net_device, gateway)
    
    # execute ip route
    exec_ipr(ipr_str)

  def ip_route_delete(self, destination_ip, destination_bit, protocol = self.protocol, table = None, net_device = None, gateway = none):
    """ delete a route from kernel routing table, via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route delete" + ip_route_gen_str(destination_ip, destination_bit, protocol, table, net_device, gateway)

    # execute ip route
    exec_ipr(ipr_str)

  def ip_route_flush(self, destination_ip = None, destination_bit = None, protocol = self.protocol, table = None, net_device = None, gateway = None):
    """ flush some routes from kernel routing table via 'ip route' """

    # check wether we have at least one valid parameter
    if((destination_ip == None) and (table == None) and (net_device == None) and (destination == None)):
      logger.error("Cannot flush without valid criteria")

    # generate the ip route string
    ipr_str = "ip route flush" + ip_route_gen_str(destination_ip, destination_bit, protocol, table, net_device, gateway)

    # execute ip route
    exec_ipr(ipr_str)

  def ip_route_change(self, destination_ip, destination_bit, protocol = self.protocol, table = None, net_device = None, gateway = None):
    """ change a route in kernel routing table via 'ip route' """

    # generate the ip route string
    ipr_str = "ip route change" + ip_route_gen_str(destination_ip, destination_bit, protocol, table, net_device, gateway)
    
    # execute ip route
    exec_ipr(ipr_str)
