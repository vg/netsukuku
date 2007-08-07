import time
from operator import itemgetter

class Neighbour:
	def __init__(self, multipath = 0, max_neigh = 16):
		self.multipath = multipath
		self.max_neigh = max_neigh
		self.rtt_variation = 0.1
		self.ip_table = {}
		self.translation_table = {}
	
	def ip_to_id(self, ipn):
		''' Trova o genera, se possibile, un id per un ip '''
		if(self.translation_table.has_key(ipn)):
			return self.translation_table[ipn]
		new_id = self.find_hole_in_tt
		if new_id:
			self.translation_table[ipn] = new_id
			return new_id
		else
			return False

	def truncate(self, ip_table):
		''' Data una ip_table prende i primi max_neigh record con l'rtt piu` basso'''
		def interesting(x):
			return x[1][1]
		trucated = []
		ip_table_trunc = {}
		counter = 0
		for key, val in sorted(ip_table.items(), reverse = False, key = interesting)
			if(counter < self.max_neigh):
				ip_table_trunc[key] = val
			else:
				if(self.ip_table.has_key(key)):
					trucated.append(key)
					self.translation_table.pop(key)
					# <genera_evento_morte>
		return truncated

	def find_hole_in_tt(self):
		''' Trova il primo interno disponibile in self.translation_table '''
		for i in xrange(self.max_neigh):
			if((i in self.translation_table) == False)
				return i
		return False

	def store(self, ip_table):
		''' Fa il merge tra l'ip_table passata e quella di Neighbour '''
		old_table = self.ip_table
		died_ip_list = []

		died_ip_list = self.truncate(ip_table)
		
		for key in self.ip_table:
			if((not (ip_table.has_key(key))) and (not (key in died_ip_list))):
				self.translation_table.pop(key)
				# <genera_evento_morte>

		for key in ip_table:
			if(not (self.ip_table.has_key(key))):
				# <genera_evento_vita>
				ip_to_id(key)
			else:
				if(abs(ip_table[key][1] - self.ip_table[key][1]) / self.ip_table[key][1] > self.rtt_mav_var):
					# <genera_evento_cambiamento>
		self.ip_table = ip_table

class Radar:
	def __init__(self, multipath = 0, bquet_num = 16, max_neigh = 16):
		self.bcast_send_time = 0
		self.bcast_arrival_time = {}
		self.bquet_dimension = bquet_num
		self.multipath = multipath
		self.broadcast = Broadcast(time_register)
		self.neigh = Neighbour(multipath, max_neigh)

	def radar(self):
		''' Manda i pacchetti in broadcast e salva nell'elenco dei vicini i risultati '''
		for i to xrange(bquet_num):
			broadcast.reply()
		self.neigh.store(self.get_all_avg_rtt())

	def reply(self):
		''' Non fa niente '''
		pass

	def time_register(self, ip, net_device):
		''' Salva l'rtt di ogni nodo '''
		time_elapsed = (int)(time.time() - bcast_send_time * 1000) / 2
		if(self.bcast_arrival_time.has_key(ip)):
			if(self.bcast_arrival_time(ip).has_key(net_device)):
				self.bcast_arrival_time[ip].append(time_elapsed)
			else:
				self.bcast_arrival_time[ip][net_device] = [time_elapsed]
		else:
			self.bcast_arrival_time[ip] = {}
			self.bcast_arrival_time[ip][net_device] = [time_elapsed]

	def get_avg_rtt(self, ip):
		''' Calcola l'rtt medio del nodo di ip "ip" '''
		if(self.multipath == 0):
			best_dev = None
			best_rtt = float("infinity"))
			for dev in self.bcast_arrival_time[ip]:
				avg = sum(self.bcast_arrival_time[node][dev]) / len(self.bcast_arrival_time[node][dev])
				if(avg <= best_time):
					best_dev = dev
					best_rtt = avg
			return [best_dev, best_rtt]
		else:
			counter = 0
			sum = 0
			for dev in self.bcast_arrival_time[ip]:
				for time in self.bcast_arrival_time[ip][dev]:
					sum += time
					counter++
			return (sum / counter)

	def get_all_avg_rtt(self):
		''' Calcola l'rtt medio di tutti i nodi'''
		all_avg = {}
		for ip in self.bcast_arrival_time:
			if(self.multipath == 0):
				all_avg[ip] = get_avg_rtt(ip)
			else:
				all_avg[ip] = [None, get_avg_rtt(ip)]
		return all_avg