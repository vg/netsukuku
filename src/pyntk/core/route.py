##
# This file is part of Netsukuku
# (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

from event import *

class Rem:
    """Route Efficiency Measure.
    
    This is a base class for different metrics (rtt, bandwidth, ...)"""

    def __init__(self, value, max_value=0, avgcoeff=1):
    	self.value=value

    	self.max_value=max_value	# Maximum value assumed by this REM
    	self.avgcoeff=avgcoeff		# Coefficient used for the average
    
    def __cmp__(self, b):
    	"""Compares two REMs
    	if remA > remB, then remA is better than remB
    	
    	NOTE: this means that if you have a list of rems and you
    	want to sort it in decrescent order of efficiency, than you
    	have to reverse sort it: list.sort(reverse=1)
    	"""
    	pass
    
    def __add__(self, b):
    	"""It sums two REMs.

    	The sum must be commutative, i.e. Rx+Ry=Ry+Rx"""
    	pass

class Rtt(Rem):
    """Round Trip Time"""

    def __init__(self, value, max_value=60*1000, avgcoeff=1):
    			  #1 minute in ms
    	Rem.__init__(self, value, max_value, avgcoeff)

    def __cmp__(self, b):
    	"""rtt comparison

    	self < b    1	 -->  The rtt `self' is better than `b'
    	self > b   -1	 -->  The rtt `self' is worse  than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value < b.value) - (self.value > b.value);
    
    def __add__(self, b):
    	return self.value+b.value

class Bw(Rem):
    """Bandwidth"""

    def __cmp__(self, b):
    	"""bandwidth comparison

    	self < b   -1	 -->  The bw `self' is worse  than `b'
    	self > b    1	 -->  The bw `self' is better than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value > b.value) - (self.value < b.value);
    
    def __add__(self, b):
    	return min(self.value, b.value)

class Avg(Rem):
    """Average"""
    
    def __init__(self, rems):
    	"""Calculates the average of different REMs.

    	`rems' is a list of type [R], where R is a Rem class, f.e. Rtt.
    	"""
    	
    	length=sum=0
    	for r in rems:
    		if not issubclass(r, Rem):
    			raise Exception, "an element of `rems' is not a Rem calss"

    		sum+=abs( r.max_value-r.value*r.avgcoeff )
    		length+=1

    	Rem.__init__(self, sum/length)

    def __cmp__(self, b):
    	"""avg comparison

    	self < b   -1	 -->  The bw `self' is worse  than `b'
    	self > b    1	 -->  The bw `self' is better than `b'
    	self = b    0	 -->  They are the same"""

    	return (self.value > b.value) - (self.value < b.value);
    
    def __add__(self, b):
    	raise Exception, "the Avg metric cannot be summed. It must be computed each time"
    	pass


class Route:
    def __init__(self, lvl, dst, gw, rem):
    	"""Initialise a route."""

    	self.lvl   = lvl
    	self.dst   = dst
    	self.gw	   = gw
    	self.rem   = rem
    
    	self.events = Event( send_ev = [ 'NEW_ROUTE', 
					 'DEL_ROUTE', 
					 'REM_ROUTE'	# the route rem changed
				       ] )

    def rem_modify(self, new_rem):
    	if self.rem != new_rem:
    		oldrem=self.rem
    		self.rem=new_rem
    		self.events.send('REM_ROUTE', (self.tuple(), oldrem))

    def tuple(self):
    	return (self.lvl, self.dst, self.gw, self.rem)

    def __del__(self):
    	if self.dst != None:
    		self.events.send('DEL_ROUTE', self.tuple())
    	self.dst=None
