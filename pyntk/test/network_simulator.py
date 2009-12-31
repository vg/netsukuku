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
# reused lukisi's code from etp_simulator.py 

# TODO: improve supporting REMs evaluation to get multiple-way 
#       network topology. See NOTE below.

import os
import sys

from random import randint

from ntk.core.radar import Neigh
from ntk.core.route import Rtt
from ntk.lib.log import init_logger

from etp_simulator import create_participant_node, initialize


sys.path.append('..')
init_logger()
initialize()

class Error(Exception): pass

bestdev = []
devs    = {}

def create_nodes(keypairs_path, localcache_path, conf_path, total_nodes=6, 
                 ip_list=[]):
    """ Create an NtkNode pool that can be used to create networks.
        The devices and REMs are ignored here, to define a topology
        use create network. """ 
    global bestdev
    global devs
    nics = ['This', 'are', 'just', 'few', 'fake', 'interfaces']
    devs = {}
    for nic in nics:
        devs[nic] = 10 # fake average value
    bestdev = ('fake', 10)
    if ip_list and total_nodes != len(ip_list):
        raise Error("There are "+str(total_nodes-ip_list)+
                                 " IPs missing in the list")
    elif not ip_list:
        # generate random IPs
        for i in xrange(total_nodes):
            ip = []
            while ip in ip_list:
                ip = [randint(1, 256) for i in range(1, 5)]
            ip_list.append(ip[:])
    
    nodes = {}
    for idn in xrange(total_nodes):            
        node_keys_path = keypairs_path + "/" + str(idn) + "/"
        node_conf_path = conf_path + "/" + str(idn) + "/"
        node_localcache_path = localcache_path + "/" + str(idn) + "/"
        if not os.path.exists(keypairs_path):
            os.mkdir(keypairs_path)
        if not os.path.exists(node_keys_path):
            os.mkdir(node_keys_path)
        else:
            files = os.listdir(node_keys_path)
            for f in files:
                os.remove(node_keys_path + "/" + f)
             
        if not os.path.exists(conf_path):
            os.mkdir(conf_path)
        if not os.path.exists(node_conf_path):
            os.mkdir(node_conf_path)
        else:
            files = os.listdir(node_keys_path)
            for f in files:
                os.remove(node_conf_path + "/" + f)
                
        if not os.path.exists(localcache_path):
            os.mkdir(localcache_path)
        if not os.path.exists(node_localcache_path):
            os.mkdir(node_localcache_path)
        else:
            files = os.listdir(node_localcache_path)
            for f in files:
                os.remove(node_localcache_path + "/" + f)
                
        # create fake snsd configuration file
        open(node_conf_path+"/snsd_nodes", "w").writelines(
                    "notappend:node"+str(idn)+":"+"7.8.9."+str(idn+11)+
                    ":0:1:1")
                                                          
        nodes[idn] = create_participant_node(ip_list[idn], 
                             nics[idn],
                             netid=123, 
                             id=idn,
                             keypair_path=keypairs_path+"/"+str(idn)+
                             "/keypair.pem",
                             localcache_path=localcache_path+"/"+str(idn)+
                             "/localcache",
                             resolv_path=conf_path+"/"+str(idn)+
                             "/resolv.conf",
                             snsd_nodes_path=conf_path+"/"+str(idn)+
                             "/snsd_nodes")
    return nodes

def _add_routes(node, nip, gw, nodes):
    """ Add routes in/from `node' to `nip' gnodes using `gw'. """
    for lvl in xrange(node.maproute.levels):
        def ntkd_func(ip, netid):
            return nodes[gw-1]
        neigh = Neigh(bestdev, 
                         devs, 
                         gw, 
                         node.maproute.nip_to_ip(nip), 
                         ntkd_func=ntkd_func)
        node.maproute.route_add(lvl, 
                         nip[lvl], 
                         neigh, 
                         Rtt(100), # not used
                         [], 
                         silent=True)
    return nodes 

# This is an example of topology, i.e. a list of
# tuples like (src, dst, gw) where src and gw are node id numbers,
# and dst is a list of node id numbers that src can reach
# through gw.
        
default_topology = [ (1, [2, 3, 4, 5, 6], 3),
                     (2, [1, 3, 4, 5, 6], 1),
                     (3, [1, 2], 2),
                     (3, [4, 5, 6], 4),
                     (4, [1, 2, 3, 5, 6], 5),
                     (5, [1, 2, 3, 4, 6], 6),
                     (6, [1, 2, 3, 4, 5], 3)]
                     
# NOTE: the topology must be one-way only, i.e. each node 
# has only one gateway to reach each other node,
# thus we don't need to manage RTTs to discriminate routes.
# (We do this to make msg_send passing acyclic).

def create_network(nodes, topology=default_topology):
    """ Create a network using nodes and topology given. """
    for (src, dst_list, gw) in default_topology:
        for dst in dst_list:
            nodes = _add_routes(nodes[src-1], nodes[dst-1].firstnip, gw, 
                                nodes)
    return nodes

def _participant_add(node, service, nip):
    """ Advise the node that nip is participant for the service given """ 
    mp = service.mapp2p
    lvl = node.maproute.nip_cmp(nip, mp.me)
    for l in xrange(lvl, mp.levels):
        if not mp.node_get(l, nip[l]).participant:
            mp.participant_node_add(l, nip[l])

# Global view of the mapp2p (see the topology).
# List of tuples like (learning_node, participant) where 
# learning_node is an id, participant is a list of ids that
# are becoming participants.

participant_map = [ (1, [2, 3, 4, 5, 6]),
                    (2, [1, 3, 4, 5, 6]),
                    (3, [1, 2, 4, 5, 6]),
                    (4, [1, 2, 3, 5, 6]),
                    (5, [1, 2, 3, 4, 6]),
                    (6, [1, 2, 3, 4, 5])]

def create_service(nodes):
    """ Make the nodes participants in ANDNA and Counter """
    for idn, participant_list in participant_map:
        for joining in participant_list:
            _participant_add(nodes[idn-1], 
                            nodes[idn-1].andna, 
                            nodes[joining-1].firstnip)
            _participant_add(nodes[idn-1], 
                            nodes[idn-1].counter, 
                            nodes[joining-1].firstnip)
                            
    for idn, node in nodes.items():
        node.counter.participate()
        node.andna.participate()
    
    return nodes