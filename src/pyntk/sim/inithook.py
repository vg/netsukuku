import __builtin__
import sys
import imp
from os import path as P

real_import = __builtin__.__import__
def wrapped_import(name, globals={}, locals={}, fromlist=[], level=-1):
#    print name, sys.path[0]
    try:
	    return real_import(name, globals, locals, fromlist, level)
    except ImportError:
	    if '.' not in name:
		    raise
	    a,b = name.split('.')
#	    print sys.path[0]+'/../'+a,b
	    m=imp.find_module(b, [sys.path[0]+'/../'+a])
#	    print name, m
	    ret=imp.load_module(name, *m)
	    return ret
__builtin__.__import__ = wrapped_import
