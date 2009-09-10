#
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


from G import *


#rem_t class

class rem_t:
	cmp_metric=G.metrics[0]
	
	def __init__(self,rtt_,bwup_,bwdw_,dp_):
		self.rtt=rtt_   #rtt in ms
		self.bwup=bwup_ #bandiwidth
		self.bwdw=bwdw_
		self.dp=dp_
		self.rem_avg_compute()		#an ad-hoc average of the previous metrics

	def rem_avg_compute(self): 
		self.avg=G.AVG_UPBW_COEF*self.bwup + G.AVG_DWBW_COEF*self.bwdw + G.AVG_RTT_COEF*(G.REM_MAX_RTT8 - self.rtt)
	
	def print_rem(self):
		print "rtt:",self.rtt, " bwup:",self.bwup," bwdw:",self.bwdw,"death_p:",self.dp,"avg:",self.avg

	
	def bw_up_cmp(self, b):
		#alessandro 1/bw modification 5-21-2007
		#return (self.bwup > b.bwup) - (self.bwup < b.bwup)
		return  (self.bwup < b.bwup) - (self.bwup > b.bwup)
	"""
         * self < b   -1	 -->  The bw self' is worse  than `new' --> APPEND b ROUTE
	 * self > b    1  	 -->  The bw self' is better than `new'
         * self = b    0	 -->  They are the same
         """	
	
	def bw_dw_cmp(self, b):
		#alessandro 1/bw modification 5-21-2007
		#return (self.bwdw > b.bwdw) - (self.bwdw < b.bwdw)
		return  (self.bwdw < b.bwdw) - (self.bwdw > b.bwdw)
		
	"""
         * self < b   -1	 -->  The bw self' is worse  than `new' --> APPEND b ROUTE
         * self > b    1   	 -->  The bw self' is better than `new'
         * self = b    0	 -->  They are the same
         """	
	
	def rtt_cmp(self, b):
		return  (self.rtt < b.rtt) - (self.rtt > b.rtt)
	
	"""
	* self > b	-1	--> The rtt self is worse than b 	-->APPEND b ROUTE
	* self < b	1	--> The rtt self is better than b	
	* self = b	0	--> They are the same
	"""
	
	def dp_cmp(self, b):
		return (self.dp < b.dp) - (self.dp > b.dp)
	"""
			we need to take the lower dp!!
         * self > b   -1	 -->  The route self is worse  than `new' --> APPEND b ROUTE
         * self < b    1   	 -->  The route self is better than `new'
         * self = b    0	 -->  They are the same
         """	
	
	
	def avg_cmp(self, b):
		return (self.avg > b.avg) - (self.avg < b.avg)
	"""
         * self < b   -1	 -->  The avg self is worse  than b
         * self > b    1  	 -->  The avg self is better than b
         * self = b    0	 -->  They are the same
        """

	def __cmp__(self,b):
		"""Remember to set rem_t.cmp_metric before comparing two
		   routes!!!"""
		if rem_t.cmp_metric not in G.metrics:
			EHM_SOMETHING_WRONG

		if rem_t.cmp_metric == "rtt":
			ret=self.rtt_cmp(b)
		elif rem_t.cmp_metric == "bwup":
			ret=self.bw_up_cmp(b)
		elif rem_t.cmp_metric == "bwdw":
			ret=self.bw_dw_cmp(b)
		elif rem_t.cmp_metric == "avg":
			ret=self.avg_cmp(b)
		elif rem_t.cmp_metric == "dp":
			ret=self.dp_cmp(b)
		else:
			EHM_SOMETHING_WRONG
		
		return ret
