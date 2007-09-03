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
import logging as logger
import re

def _exec_ipl(ipl_str):
    """ ipl_str: ip link string (or ip addr) """
      
    # execute ip link/addr
    status, output = commands.getstatusoutput(ipl_str)
                
    # check if everything went good.. if not, log!
    if(status != 0):
      logger.error(output)
      
    return output

class Nic:
  """ this class is used to manage network interfaces """

  def __init__(self, devname):
    """ devname: nic's name """
    
    self.devname = devname

  def up(self):
    """ bring up the interface """
    self._ip_link_updown("up")
  
  def down(self):
    """ bring down the interface """
    self._ip_link_updown("down")
    
  def change_address(self, address):
    """ change the interface's address """
    self._ip_link_change_address(address)
    
  def retrieve_info(self, ip_version):
    """ return the following tuple:
        (up_or_down, address)
        up_or_down: True (nic is up) | False (nick is down)
        address: nic's address
    """
    return self._ip_address_show(ip_version)

  def activate(self, address):
    """ Brings down and up the interface and changes its address to `address' """
    self.down()
    self.up()
    self.change_address(address)
    
  def _ip_address_show(self, ip_version):
    """ return infos about the interface, obtained via ip addr """
    
    # generate ip addr string
    ipa_str = "ip addr show dev " + str(self.devname)
    
    # execute ip addr
    ipa_out = _exec_ipl(ipa_str)
    
    # check wether the interface is up or down
    up_reg = re.compile("\Wup\W", re.IGNORECASE)
    up_mat = up_reg.search(ipa_out)
    if up_mat is None:
      up_or_down = False
    else:
      up_or_down = True
    
    # check wether we're using IPv6 or not
    v6_reg = re.compile("inet6")
    v6_mat = v6_reg.search(ipa_out)
    if v6_mat is None:
      if ip_version == 6:
        logger.error("No IPv6 address associated with " + str(self.devname))
        return (None, None)

    # find the ip address
    if ip_version == 6:
      ip_reg = re.compile("inet6 (.*:){5}(.*)\/")
      ip_mat = ip_reg.search(ipa_out)
      if ip_mat is None:
        logger.error("No valid IPv6 address found for " + str(self.devname))
        return (None, None)
      else:
        raw_address = ip_mat.group()
        # remove "inet6 " and "/"
        address = raw_address[6:-1]
    else:
      ip_reg = re.compile("inet (.*\.){3}(.*)\/")
      ip_mat = ip_reg.search(ipa_out)
      if ip_mat is None:
        logger.error("No valid IPv4 address found for " + str(self.devname))
        return (None, None)
      else:
        raw_address = ip_mat.group()
        # remove "inet " and "/"
        address = raw_address[5:-1]
    
    return (up_or_down, address)

  def _ip_link_updown(self, up_or_down):
    """ bring up/down the interface via ip link """
    
    # generate ip link string
    ipl_str = "ip link set dev " + str(self.devname) + " " + str(up_or_down)
    
    # execute ip link
    _exec_ipl(ipl_str)

  def _ip_link_change_address(self, address):
    """ change the interface's address via ip addr """
    
    # flush all the addresses 
    ipl_str1 = "ip addr flush dev " + str(self.devname)
    # add the new one
    ipl_str2 = "ip addr add "+ str(address) +" dev " + str(self.devname)
    
    # execute
    _exec_ipl(ipl_str1)
    _exec_ipl(ipl_str2)

class NicAll:
    """Class to manage all the network interfaces"""

    def __init__(self, nics=[], exclude_nics=['lo']):
        
        self.nic_names = nics
        
	if nics == []:
    	    self.nic_names = self.nics_list()
	for en in exclude_nics:
		self.nic_names.remove(en)
        
	self.nics = map(Nic, self.nic_names)

    def nics_list(self):
        """Returns the list of the names of all the nic that are currently
	   up"""

        # generate ip addr string
        ipl_str = "ip link show"
        ipl_out = _exec_ipl(ipl_str)

        rec=re.compile(r"^[0-9]+: (\w+):.*\WUP\W")
    	return [ m.group(1) for l in ipl_out.splitlines()
				for m in [rec.match(l)] 
					if m != None ]
    
    def up(self):
        """Brings up all the interfaces"""
	for n in self.nics:
    		n.up()
    
    def down(self):
        """Brings up all the interfaces"""
	for n in self.nics:
    		n.down()

    def retrieve_info(self, ip_version):
	return [ (n.devname,)+n.retrieve_info(ip_version) for n in self.nics]

    def change_address(self, address):
        """Change the address to all the interfaces """
	for n in self.nics:
		n.change_address(address)
    
    def activate(self, address):
        """Activate all the interfaces"""
	for n in self.nics:
		n.activate(address)

    
if __name__ == "__main__":
	n=Nic("dummy0")
	n.up()
	print n.retrieve_info(4)
	n.down()
	print n.retrieve_info(4)
	n.change_address("11.22.33.44")
	print n.retrieve_info(4)
	n.activate("11.22.33.55")
	print n.retrieve_info(4)

	na=NicAll()
	print na.retrieve_info(4)
