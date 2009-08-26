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


import sys
sys.path.append('..')
from ntk.lib.micro import micro, microfunc, allmicro_run, Channel, micro_block
import stackless

@microfunc()
def recv(ch, name):
 print "Started recv<%s>" % (name,)
 print "recv<%s>: %r" % (name, ch.bcast_recv())

def testBroadcastChannel():
    '''Taken from http://zope.stackless.com/wiki/ChannelExamples'''
    print
    print "testBroadcastChannel"
    print "--------------------"
    ch = Channel()

    # Essentially nonblocking on sends when there are no receivers
    ch.bcast_send("broadcast 1")

    for name in "ABCDE":
        print 'recv(ch, name)', name
        recv(ch, name)
    
    ch.bcast_send("broadcast 2")
    while 1:
        micro_block()
    print

def testBroadcastChannel2():
    print
    print "testBroadcastChannel"
    print "--------------------"
    ch = Channel(broadcast=True)

    # Essentially nonblocking on sends when there are no receivers
    ch.send("Test when empty")

    for name in "ABCDE":
        task = stackless.tasklet(recv)(ch, name)
        task.run()

    ch.send("broadcast from host")
    print



testBroadcastChannel()
allmicro_run()
