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

import sys
sys.path.append("..")
from ntkd       import NtkdBroadcast
from lib.xtime  import swait, time
from lib.micro  import micro
from lib.event  import Event
from core.route import Rtt
from operator   import itemgetter

class NodeInfo:
  """ this class store informations about a node """
  def __init__(self, net_device, rtt, ntk):
    """ net_device: node's nic
        rtt: node's round trip time
        ntk: node's ntk remote instance
    """
    self.dev = net_device
    self.rtt = rtt
    self.ntk = ntk

class Neigh:
  """ this class simply represent a neighbour """

  def __init__(self, ip, idn, rtt, netid):
    """ ip: neighbour's ip;
        idn: neighbour's id;
        rtt: neighbour's round trip time;
	netid: network id of the node
    """

    self.ip = ip
    self.id = idn
    self.rem = Rtt(rtt)		# TODO(low): support the other metrics
    # neighbour's ntk remote instance
    self.ntk = Ntk(ip)
    self.netid = netid

class Neighbour:
  """ this class manages all neighbours """
  def __init__(self, multipath = 0, max_neigh = 16):
    """ multipath: does the current kernel we're running on support multipath routing?;
        max_neigh: maximum number of neighbours we can have
    """

    self.multipath = multipath
    self.max_neigh = max_neigh
    # variation on neighbours' rtt greater than this will be notified
    self.rtt_variation = 0.1
    # ip_table
    self.ip_table = {}
    # IP => ID translation table
    self.translation_table = {}
    # IP => netid
    self.netid_table = {}
    # the events we raise
    self.events = Event(['NEW_NEIGH', 'DEL_NEIGH', 'REM_NEIGH'])

  def neigh_list(self):
    """ return the list of neighbours """
    nlist = []
    for key, val in ip_table:
      nlist.append(Neigh(key, translation_table[key], val.rtt, self.netid_table[key]))
    return nlist

  def ip_to_id(self, ipn):
    """ if ipn is in the translation table, return the associated id;
        if it isn't, insert it into the translation table assigning a new id,
        if the table isn't full
    """

    if(ipn in self.translation_table):
      return self.translation_table[ipn]
    new_id = self._find_hole_in_tt()
    if new_id:
      self.translation_table[ipn] = new_id
      return new_id
    else:
      return False

  def ip_to_neigh(self, ip):
    """ ip: neighbour's ip
        return a Neigh object from an ip
    """
    
    return Neigh(ip, self.translation_table[ip], self.ip_table[ip].rtt,
		    self.netid_table[ip])

  def _truncate(self, ip_table):
    """ ip_table: an {IP => NodeInfo ~ [dev, rtt, ntk]};
        we want the best (with the lowest rtt) max_neigh nodes only to remain in the table
    """

    # auxiliary function, to take rtt from {IP => NodeInfo}
    def interesting(x):
      return x[1].rtt

    # remember who we are truncating
    trucated = []

    # the new table, without truncated rows
    ip_table_trunc = {}

    # a counter
    counter = 0

    # we're cycling through ip_table, ordered by rtt
    for key, val in sorted(ip_table.items(), reverse = False, key = interesting):
      # if we haven't still reached max_neigh entries in the new ip_table
      if(counter < self.max_neigh):
        # add the current row into ip_table
        ip_table_trunc[key] = val
      else:
        # otherwise just drop it
        # but, if old ip_table contained this row, we should notify our listeners about this:
        if(key in self.ip_table):
          # remember we are truncating this row
          trucated.append(key)
	  # delete the entry
	  self.delete(key, remove_from_iptable=False)

    # return the new ip_table and the list of truncated entries
    return (ip_table_trunc, truncated)

  def _find_hole_in_tt(self):
    """find the first available index in translation_table"""
    for i in xrange(self.max_neigh):
      if((i in self.translation_table.values()) == False):
        return i
    return False

  def store(self, ip_table):
    """ ip_table: the new ip_table;
        substitute the old ip_table with the new and notify about the changes
    """

    # the rows deleted during truncation
    died_ip_list = []

    (ip_table, died_ip_list) = self._truncate(ip_table)

    # first of all we cycle through the old ip_table
    # looking for nodes that aren't in the new one
    for key in self.ip_table:
      # if we find a row that isn't in the new ip_table and whose
      # deletion hasn't already been notified (raising an event)
      # during truncation
      if((not (key in ip_table)) and (not (key in died_ip_list))):
	  self.delete(key, remove_from_iptable=False)

    # now we cycle through the new ip_table
    # looking for nodes who weren't in the old one
    # or whose rtt has sensibly changed
    for key in ip_table:
      # if a node has been added
      if(not (key in self.ip_table)):
        # generate an id and add the entry in translation_table
        self.ip_to_id(key)
        # send a message notifying we added a node
        self.events.send('NEW_NEIGH', 
			 (Neigh(key, self.translation_table(key),
				self.ip_table[key].rtt, self.netid_table[key])))
      else:
        # otherwise (if the node already was in old ip_table) check if
        # its rtt has changed more than rtt_variation
        if(abs(ip_table[key].rtt - self.ip_table[key].rtt) / self.ip_table[key].rtt > self.rtt_mav_var):
          # send a message notifying the node's rtt changed
          self.events.send('REM_NEIGH',
			   (Neigh(key, self.translation_table[key], ip_table[key].rtt), 
				   self.ip_table[key].rtt,
				   self.netid_table[key]))

    # finally, update the ip_table
    self.ip_table = ip_table

  def readvertise(self):
    """Sends a NEW_NEIGH event for each stored neighbour"""
    for key in ip_table:
        self.events.send('NEW_NEIGH', 
			 (Neigh(key, self.translation_table(key),
				self.ip_table[key].rtt, self.netid_table[key])))

  def delete(self, ip, remove_from_iptable=True):
    """Deletes an entry from the ip_table"""
    if ip in self.ip_table and remove_from_iptable:
	    del self.ip_table[ip]
    # delete the entry from the translation table...
    old_id = self.translation_table.pop(ip)
    # ...and from the netid_table
    old_netid = self.netid_table.pop(ip)
    # send a message notifying we deleted the entry
    self.events.send('DEL_NEIGH', (Neigh(ip, old_id, None, old_netid)))

class Radar:
  def __init__(self, multipath = 0, bquet_num = 16, max_neigh = 16, max_wait_time = 8):
    """ multipath: does the current kernel we're running on support multipath routing?;
        bquet_num: how many packets does each bouquet contain?;
        max_neigh: maximum number of neighbours we can have;
        max_wait_time: the maximum time we can wait for a reply, in seconds;
    """

    # how many bouquet we have already sent
    self.bouquet_numb = 0
    # when we sent the broadcast packets
    self.bcast_send_time = 0
    # when the replies arrived
    self.bcast_arrival_time = {}
    self.bquet_dimension = bquet_num
    self.multipath = multipath
    self.max_wait_time = max_wait_time
    # an instance of the NtkdBroadcast class to manage broadcast sending
    self.broadcast = NtkdBroadcast(self.time_register)
    # our neighbours
    self.neigh = Neighbour(multipath, max_neigh)

    # Send a SCAN_DONE event each time a sent bouquet has been completely
    # collected
    self.events = Event( [ 'SCAN_DONE' ] )

    # Our netid. It's a random id used to detect network collisions.
    self.netid = None

  def run(self, started=0):
    if not started:
      micro(self.radar_run, started=1)
    else:
      while True: self.radar()

  def radar(self):
    """ Send broadcast packets and store the results in neigh """

    # we're sending the broadcast packets NOW
    self.bcast_send_time = xtime.time()

    # send all packets in the bouquet
    def br():
      self.broadcast.reply()
    for i in xrange(bquet_num):
      micro(br)

    # then wait
    swait(self.max_wait_time * 1000)

    # update the neighbours' ip_table
    self.neigh.store(self.get_all_avg_rtt())

    # Send the event
    self.bouquet_numb+=1
    self.events.send('SCAN_DONE', (bouquet_numb))

  def reply(self):
    """ As answer we'll return our netid """
    return self.netid

  def time_register(self, ip, net_device, msg):
    """save each node's rtt"""

    # this is the rtt
    time_elapsed = int(xtime.time() - bcast_send_time / 2)
    # let's store it in the bcast_arrival_time table
    if(ip in self.bcast_arrival_time):
      if(net_device in self.bcast_arrival_time[ip]):
        self.bcast_arrival_time[ip].append(time_elapsed)
      else:
        self.bcast_arrival_time[ip][net_device] = [time_elapsed]
    else:
      self.bcast_arrival_time[ip] = {}
      self.bcast_arrival_time[ip][net_device] = [time_elapsed]
    
    self.neigh.netid_table[ip] = msg


  def get_avg_rtt(self, ip):
    """ ip: an ip;
        calculate the average rtt of ip
    """

    if(self.multipath == 0):
      # if we can't use multipath routing use the value from the best nic
      best_dev = None
      best_rtt = float("infinity")
      # for each nic
      for dev in self.bcast_arrival_time[ip]:
        # calculate the average rtt
        avg = sum(self.bcast_arrival_time[node][dev]) / len(self.bcast_arrival_time[node][dev])
        # and check if it's the current best
        if(avg <= best_time):
          best_dev = dev
          best_rtt = avg
      # finally return which nic had the best average rtt and what was it
      return (best_dev, best_rtt)
    else:
      # otherwise use the value from all the nics
      counter = 0
      sum = 0
      # for each nic
      for dev in self.bcast_arrival_time[ip]:
        # for each time measurement
        for time in self.bcast_arrival_time[ip][dev]:
          # just add it to the total sum
          sum += time
          # and update the counter
          counter += 1
      # finally return the average rtt
      return (sum / counter)

  def get_all_avg_rtt(self):
    """calculate the average rtt of all the ips"""

    all_avg = {}
    # for each ip
    for ip in self.bcast_arrival_time:
      # if we can't use multipath routing
      if(self.multipath == 0):
        # simply store get_avg_rtt's value
        a_dev, a_rtt = get_avg_rtt(ip)
        all_avg[ip] = NodeInfo(a_dev, a_rtt, Ntk(ip))
      else:
        # otherwise, set None as the device
        all_avg[ip] = NodeInfo(None, get_avg_rtt(ip), Ntk(ip))
    return all_avg

