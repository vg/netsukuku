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

class Nic:
  """ this class is used to manage network interfaces """
  def __init__(self, net_device):
    """ net_device: nic's name """
    
    self.net_device = net_device

  def _exec_ipl(self, ipl_str):
    """ ipl_str: ip link string """
      
    # execute ip link
    status, output = commands.getstatusoutput(ipl_str)
                
    # check if everything went good.. if not, log!
    if(status != 0):
      logger.error(output)                            

  def nic_up(self):
    """ bring up the interface """
    _ip_link_nic_updown("up")
  
  def nic_down(self):
    """ bring down the interface """
    _ip_link_nic_updown("down")
    
  def nic_change_address(self, address):
    """ change the interface's address """
    _ip_link_nic_change_address(address)
    
  def _ip_link_nic_up(self, up_or_down):
    """ bring up/down the interface via ip link """
    
    # generate ip link string
    ipl_str = "ip link set dev " + str(self.net_device) + " " + str(up_or_down)
    
    # execute ip link
    _exec_ipl(ipl_str)

  def _ip_link_change_address(self, address):
    """ change the interface's address via ip link """
    
    # generate ip link string
    ipl_str = "ip link set dev " + str(self.net_device) + " address " + str(address)
    
    # execute ip link
    _exec_ipl(ipl_str)
