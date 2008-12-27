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
#
# Wrap code.
# First try to load a .py module from sim/. If it fails, try to import a real
# .py module from ntk/
#


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
