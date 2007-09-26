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
def crecv(ch):
	print 'crecv start'
	r=ch.recv()
	print r
	ch.send('got')
	print 'crecv end'

@microfunc(True)
def csend(ch):
	print 'csend start'
	ch.send('take')
	def xf():print 'xf1'
	micro(xf)
	print ch.recv()
	print 'csend end'

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

c=Channel()
csend(c)
def xf():print 'xf2'
micro(xf)
crecv(c)

test_sequence_II()

allmicro_run()

T2=T[:]

print T1
assert T1 == range(1,9)
print T2
assert T2 == range(1,9)

print "all ok"
