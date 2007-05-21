#  This file is part of Netsukuku, it is based on q2sim.py
#
#  ntksim
#  (c) Copyright 2007 Alessandro Ordine
#
#  q2sim.py
#  (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
# 
#  This source code is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as published 
#  by the Free Software Foundation; either version 2 of the License,
#  or (at your option) any later version.
# 
#  This source code is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  Please refer to the GNU Public License for more details.
# 
#  You should have received a copy of the GNU Public License along with
#  this source code; if not, write to:
#  Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# 

#This file contains all global objects


class G:
	#
	# Defines
	#
	DEFAULT_RTT = 100 #ms
	DEFAULT_BW = 50 #kbps	
	DELTA_RTT = DEFAULT_RTT/10
	
	AVG_UPBW_COEF=1.0/4
	AVG_DWBW_COEF=1.0/4
	AVG_RTT_COEF=2.0/4
	REM_MAX_RTT8=255
	

	NUM_OF_LEVELS=4
	MAXGROUPNODE=3
	STARTER_NODES=1	#number of nodes that start to explore the network simultaniously
	LEVEL_1=0
	LEVEL_2=1
	LEVEL_3=2
	LEVEL_4=3
	
	DP_ENHANCEMENT=1	#enable the dynamic capabilities of the nodes 
	LAMBDA=10.42		#to be computed, 10.42 allows to have the 20% of the nodes with a 
				#death probability between 7% and 10%
	
	network={} #we will use network[(LEVEL,GNODE_ID)]=map_gnode
	whole_network={}	# Each element is { nid: relative node class }
	splitted=[]
	#
	# Globals
	#
	curtime=0
	events=[]
	EVENTS_LIMIT=0
	MAX_ROUTES=1
	#metrics=["rtt","bwup","bwdw","avg","dp"]
	metrics=["rtt"]
	total_packets_forwarded=0
	
	#
	# Flags
	#
	verbose=False
	events_limit_reached=False
