def remove_duplicates(a):
	return [ x for i, x in enumerate(a) if i==a.index(x) ]
