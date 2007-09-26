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

from random import randint

from ntk.wrap.xtime  import swait, time
from ntk.lib.micro  import micro
from ntk.lib.event  import Event
from ntk.core.route import Rtt
import ntk.lib.rpc as rpc

class NodeInfo(object):
  """ this class store informations about a node """
  __slots__ = ['devs', 'bestdev', 'ntk']
  def __init__(self, devs, bestdev, ntk):
    """ devs: a dict which maps a device to the average rtt
        bestdev: a pair (d, avg_rtt), where devs[d] is the best element of
                 devs.
        ntk: node's ntk remote instance
    """
    self.devs = devs
    self.bestdev = bestdev
    self.ntk = ntk

class Neigh:
  """ this class simply represent a neighbour """

  __slots__ = ['devs', 'bestdev', 'ip', 'id', 'rem', 'ntk', 'netid']

  def __init__(self, ip, idn, devs, bestdev, netid):
    """ ip: neighbour's ip;
        idn: neighbour's id;
        devs: a dict which maps a device to the average rtt
        bestdev: a pair (d, avg_rtt), where devs[d] is the best element of
                 devs.
        netid: network id of the node
    """

    self.devs = devs
    self.bestdev = bestdev

    self.ip = ip
    self.id = idn
    self.rem = Rtt(self.bestdev[1])             # TODO(low): support the other metrics
    # neighbour's ntk remote instance
    self.ntk = Ntk(ip)
    self.netid = netid

class Neighbour:
  """ this class manages all neighbours """
  __slots__ = ['max_neigh', 'rtt_variation', 'ip_table', 'translation_table',
                  'netid_table', 'events', 'remotable_funcs']

  def __init__(self, max_neigh = 16):
    """  max_neigh: maximum number of neighbours we can have
    """
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
    self.events = Event(['NEIGH_NEW', 'NEIGH_DELETED', 'NEIGH_REM_CHGED'])

    self.remotable_funcs = [self.ip_change]

  def neigh_list(self):
    """ return the list of neighbours """
    nlist = []
    for key, val in self.ip_table:
      nlist.append(Neigh(key, translation_table[key], val.devs, val.bestdev, self.netid_table[key]))
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
    if ip not in self.translation_table:
            return None
    else:
            return Neigh(ip, self.translation_table[ip], 
                            self.ip_table[ip].devs,
                            self.ip_table[ip].bestdev,
                            self.netid_table[ip])

  def id_to_ip(self, id):
    """ Returns the IP associated to `id'.
        If not found, returns None"""
    for ip in self.translation_table:
            if self.translation_table[ip] == id:
                    return ip
    return None
 
  def id_to_neigh(self, id):
    """ Returns a Neigh object from an id """
    return self.ip_to_neigh(self.id_to_ip(id))

  def _truncate(self, ip_table):
    """ ip_table: an {IP => NodeInfo};
        we want the best (with the lowest rtt) max_neigh nodes only to remain in the table
    """

    # auxiliary function, to take rtt from {IP => NodeInfo}
    def interesting(x):
      return x[1].bestdev[1]

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
        self.events.send('NEIGH_NEW', 
                         (Neigh(key, self.translation_table(key),
                                self.ip_table[key].devs,
                                self.ip_table[key].bestdev,
                                self.netid_table[key])))
      else:
        # otherwise (if the node already was in old ip_table) check if
        # its rtt has changed more than rtt_variation
        if(abs(ip_table[key].bestdev[0][1] - self.ip_table[key].bestdev[1]) /
                        self.ip_table[key].bestdev[1] > self.rtt_variation):
          # send a message notifying the node's rtt changed
          self.events.send('NEIGH_REM_CHGED',
                           (Neigh(key, self.translation_table[key],
                                       self.ip_table[key].devs,
                                       self.ip_table[key].bestdev,
                                       self.netid_table[key]) ))

    # finally, update the ip_table
    self.ip_table = ip_table

  def readvertise(self):
    """Sends a NEIGH_NEW event for each stored neighbour"""
    for key in ip_table:
        self.events.send('NEIGH_NEW', 
                         (Neigh(key, self.translation_table(key),
                                       self.ip_table[key].devs,
                                       self.ip_table[key].bestdev,
                                       self.netid_table[key])))

  def delete(self, ip, remove_from_iptable=True):
    """Deletes an entry from the ip_table"""
    if remove_from_iptable:
            del self.ip_table[ip]
    # delete the entry from the translation table...
    old_id = self.translation_table.pop(ip)
    # ...and from the netid_table
    old_netid = self.netid_table.pop(ip)
    # send a message notifying we deleted the entry
    self.events.send('NEIGH_DELETED', (Neigh(ip, old_id, None, None, old_netid)))

  def ip_change(self, oldip, newip):
    """Adds `newip' in the Neighbours as a copy of `oldip', then it removes
       `oldip'. The relative events are raised."""

    self.ip_table[newip]         = self.ip_table[oldip]
    self.translation_table[newip]= self.translation_table[oldip]
    self.netid_table[newip]      = self.netid_table[oldip]

    self.events.send('NEIGH_NEW', (
                      Neigh(newip, self.translation_table(newip),
                                   self.ip_table[newip].devs,
                                   self.ip_table[newip].bestdev,
                                   self.netid_table[newip]) ))
    self.delete(oldip)


class Radar:
  __slots__ = ['inet', 'bouquet_numb', 'bcast_send_time',
    'bcast_arrival_time', 'bquet_dimension', 'max_wait_time', 'broadcast',
    'neigh', 'events', 'netid', 'do_reply', 'remotable_funcs', 'ntkd_id' ]
    
  def __init__(self, inet, bquet_num = 16, max_neigh = 16, max_wait_time = 8):
    """
        inet:   network.inet.Inet instance
        bquet_num: how many packets does each bouquet contain?;
        max_neigh: maximum number of neighbours we can have;
        max_wait_time: the maximum time we can wait for a reply, in seconds;
    """
   
    self.inet = inet

    # how many bouquet we have already sent
    self.bouquet_numb = 0
    # when we sent the broadcast packets
    self.bcast_send_time = 0
    # when the replies arrived
    self.bcast_arrival_time = {}
    self.bquet_dimension = bquet_num
    self.max_wait_time = max_wait_time
    # an instance of the RPCBroadcast class to manage broadcast sending
    self.broadcast = rpc.BcastClient(self.inet)
    # our neighbours
    self.neigh = Neighbour(max_neigh)

    # Send a SCAN_DONE event each time a sent bouquet has been completely
    # collected
    self.events = Event( [ 'SCAN_DONE' ] )

    # Our netid. It's a random id used to detect network collisions.
    self.netid = -1

    # If set to True, this module will reply to radar queries sent by our
    # neighbours.
    self.do_reply = False

    self.remotable_funcs = [self.reply, self.time_register]

    self.ntkd_id = randint(0, 2**32-1)
  
  def run(self, started=0):
    if not started:
      micro(self.run, (1,))
    else:
      while True: self.radar()

  def radar(self):
    """ Send broadcast packets and store the results in neigh """

    self.radar_id = randint(0, 2**32-1)

    # we're sending the broadcast packets NOW
    self.bcast_send_time = time()

    # send all packets in the bouquet
    for i in xrange(bquet_num):
      self.broadcast.radar.reply(self.ntkd_id, self.radar_id)

    # then wait
    swait(self.max_wait_time * 1000)

    # update the neighbours' ip_table
    self.neigh.store(self.get_all_avg_rtt())

    # Send the event
    self.bouquet_numb+=1
    self.events.send('SCAN_DONE', (bouquet_numb))

  def reply(self, _rpc_caller, ntkd_id, radar_id):
    """ As answer we'll return our netid """
    if self.do_reply and ntkd_id != self.ntkd_id:
            rpc.BcastClient(devs=[_rpc_caller.dev]).radar.time_register(radar_id, self.netid)
            return self.netid

  def time_register(self, _rpc_caller, radar_id, netid):
    """save each node's rtt"""

    if radar_id != self.radar_id:
            # drop. It isn't a reply to our current bouquet
            return

    ip = self.inet.str_to_ip(_rpc_caller.ip)
    net_device = _rpc_caller.dev

    # this is the rtt
    time_elapsed = int((time() - bcast_send_time) / 2)
    # let's store it in the bcast_arrival_time table
    if(ip in self.bcast_arrival_time):
      if(net_device in self.bcast_arrival_time[ip]):
        self.bcast_arrival_time[ip].append(time_elapsed)
      else:
        self.bcast_arrival_time[ip][net_device] = [time_elapsed]
    else:
      self.bcast_arrival_time[ip] = {}
      self.bcast_arrival_time[ip][net_device] = [time_elapsed]
    
    self.neigh.netid_table[ip] = netid


  def get_avg_rtt(self, ip):
    """ ip: an ip;
        Calculates the average rtt of IP for each device

        Returns the ordered list [(dev, avgrtt)], the first element has the
        best average rtt.
    """
    
    devlist = []

    # for each nic
    for dev in self.bcast_arrival_time[ip]:
        avg = sum(self.bcast_arrival_time[ip][dev]) / len(self.bcast_arrival_time[ip][dev])
        devlist.append( (dev, avg) )

    # sort the devices, the best is the first
    def second_element((x,y)): return y
    devlist.sort(key=second_element)
    
    return devlist

  def get_all_avg_rtt(self):
    """calculate the average rtt of all the ips"""

    all_avg = {}
    # for each ip
    for ip in self.bcast_arrival_time:
        devs = get_avg_rtt(ip)
        all_avg[ip] = NodeInfo(dict(devs), devs[0], Ntk(ip))
    return all_avg