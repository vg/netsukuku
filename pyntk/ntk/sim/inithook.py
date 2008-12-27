import __builtin__
import sys
import imp
#from os import path as P

real_import = __builtin__.__import__
def wrapped_import(name, globals={}, locals={}, fromlist=[], level=-1):
    #print name, sys.path[0], P.abspath(P.curdir)
    try:
        return real_import(name, globals, locals, fromlist, level)
    except ImportError:
        if '.' not in name:
            raise
        #print '#'*80
        #print name
        #print '#'*80
        a,b,c = name.split('.')
        #print sys.path[0]+'/../'+a,b
        m=imp.find_module(b, [sys.path[0]+'/../'+a])
        #print m
        return imp.load_module(name, *m)

__builtin__.__import__ = wrapped_import
