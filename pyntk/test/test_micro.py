##
# This file is part of Netsukuku
# (c) Copyright 2008 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
sys.path.append('..')
from ntk.lib.micro import micro, microfunc, allmicro_run, Channel

T=T1=T2=[]

@microfunc()
def mf(x,y):
        print "MFxy", x,y
        T.append(x)

def f(x,y):
        print "fxy", x,y
        T.append(x)

@microfunc(True)
def mft(x,y):
        print "MFTxy", x,y
        T.append(x)

@microfunc()
def mvoid():
        print "mvoid"

class foo:
    def __init__(self, a):
            self.a=a
    @microfunc()
    def void(self):
        print "foovoid", self.a

@microfunc(True)
def crecv(ch,n):
        print 'crecv start',n
        r=ch.recv()
        print 'crecv',n, ' received:', r
        ch.send('got')
        print 'crecv end',n

@microfunc(True)
def csend(ch,n):
        print 'csend start',n
        ch.send('take')
        print 'csend sent', n
        def xf():print 'csend xf1',n
        micro(xf)
        print 'csend',n,' ch.recv: ',ch.recv()
        print 'csend end',n

@microfunc(is_atomic=True)
def csend_atomic(ch, n):
        print 'csend_atomic start',n
        ch.send('take the atom')
        print 'csend_atomic sent', n
        def xf():print 'csend_atomic xf1',n
        micro(xf)
        print 'csend_atomic ch.recv',n,': ',ch.recv()
        print 'csend_atomic end',n


mf(1,1)
micro(f, (4,4))
mft(5,5)
micro(f, (6,6))
mf(2,2)
mft(7,7)
mf(3,3)
micro(f, (8,8))

@microfunc(1)
def test_sequence_II():
        global T, T1, T2
        T1=T[:]
        T=[]
        mf(1,1)
        micro(f, (4,4))
        mft(5,5)
        micro(f, (6,6))
        mf(2,2)
        mft(7,7)
        mf(3,3)
        micro(f, (8,8))

mvoid()

F1=foo(1)
F2=foo(2)
F1.void()
F2.void()

def xf():print 'xf2',1
c=Channel()
csend(c, 1)
micro(xf)
crecv(c,1)

#print '---Atomic test---'
def xf():print 'xf4',3
c=Channel(prefer_sender=True)
crecv(c,3)
micro(xf)
csend_atomic(c,3)

#print '--- Channel test 2 ---'
c=Channel()
def xf():print 'xf3',2
crecv(c, 2)
micro(xf)
csend(c, 2)


test_sequence_II()

allmicro_run()

T2=T[:]

print T1
assert T1 == range(1,9)
print T2
assert T2 == range(1,9)

print "all ok"
