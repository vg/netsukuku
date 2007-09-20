import __builtin__
import sys
import imp
from os import path as P

real_import = __builtin__.__import__
def wrapped_import(name, globals={}, locals={}, fromlist=[], level=-1):
#    print name, sys.path[0], P.abspath(P.curdir)
    try:
	    return real_import(name, globals, locals, fromlist, level)
    except ImportError:
	    a,b = name.split('.')
	    spath = P.split(P.dirname(sys.path[0]))[1]
	    mpath = P.join(P.abspath(P.curdir), spath, P.pardir, a)
	    m=imp.find_module(b, [sys.path[0]+'/../'+a])
#	    print m
	    return imp.load_module(name, *m)
__builtin__.__import__ = wrapped_import
