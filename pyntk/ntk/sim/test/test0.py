import sys

import traceback
import random
from random import randint
import pdb

from ntk.sim.net import Net
import ntk.sim.sim
import ntk.sim.lib.xtime as xtime
from ntk.lib.micro import micro, microfunc, allmicro_run, Channel

ch = Channel()
ch2 = Channel(True)
ch3 = Channel()

T=[]

def myxtime():
	t=xtime.time()
	T.append(t)
	return t

@microfunc(True)
def f(x):
	ch2.recv()
	print 'ch2'
	print "t:", myxtime()
	f(x)

@microfunc(True)
def fe(x):
    ch.recv()
    print 'ch1'
    print "t:", myxtime(), 'sending in ch2'
#    micro(ch2.send, (3,))
    ch2.send(3)
    print "t:", myxtime(), 'sent in ch2'
    sim.cursim.ev_add(sim.SimEvent(2, ch2.send, (2,)))
    sim.cursim.ev_add(sim.SimEvent(2, ch3.send, (3,)))

@microfunc(True)
def fx(x):
    ch3.recv()
    print 'ch3'
    print "t:", myxtime()
    fx(x)

sim.sim_activate()

f(0)
fe(1)
fx(2)

sim.cursim.ev_add(sim.SimEvent(10, ch.send, (1,)))
sim.cursim.ev_add(sim.SimEvent(70, ch3.send, (1,)))

sim.sim_run()
allmicro_run()

print T
assert T == [10, 10, 12,12,12, 70]
print "all ok"
