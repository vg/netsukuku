##
# This file is part of Netsukuku
# (c) Copyright 2007 Francesco Losciale aka jnz <frengo@anche.no>
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

import traceback
import logging as stdlogging

from datetime import datetime


def caller(up=0):
     '''Get file name, line number, function name and
        source text of the caller's caller as 4-tuple:
        (file, line, func, text).

        The optional argument 'up' allows retrieval of
        a caller further back up into the call stack.

        Note, the source text may be None and function
        name may be '?' in the returned result.  In
        Python 2.3+ the file name may be an absolute
        path.
     '''
     try:  # just get a few frames
         f = traceback.extract_stack(limit=up+2)
         if f:
            return f[0]
     except:
         if __debug__:
            traceback.print_exc()
         pass
      # running with psyco?
     return ('', 0, '', None)


def debug(msg, *args):
    if args:
        msg = msg % args
    curr = datetime.utcnow()
    min, sec = curr.minute, curr.second
    (file, line, func, text) = caller(1)
    file = file.split("/ntk/")[1]
    stdlogging.debug("[{0:2}:{1:2}] from {2:5} @ {3:30} {4}"
                     .format(min, sec, line, file, msg))
    
