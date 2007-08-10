def dict_remove_none(d):
    d2={}
    for key in d:
	    if d[key] != None:
		    d2[key]=d[key]
    return d2
