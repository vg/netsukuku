import sys
sys.path.append('..')
from lib.micro import micro, microfunc, allmicro_run

T=[]

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

mf(1,1)
micro(f, (2,2))
mft(4,4)
micro(f, (5,5))
mf(3,3)
mft(7,7)
mf(6,6)
micro(f, (8,8))

allmicro_run()

assert T == range(1,9)

print "all ok"
