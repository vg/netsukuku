def remove_duplicates(a):
	return [ x for i, x in enumerate(a) if i==a.index(x) ]

def dec2bin(x):
	b=''
	i=0
	x=int(x)
	while 1:
		b+=str(x&1)
		x>>=1
		i+=1
		if x <= 0:
			break
	while i < 8:
		b+='0'
		i+=1
	return b[::-1]

