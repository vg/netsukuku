#!/usr/bin/env python
##
# This file is part of Netsukuku
# (c) Copyright 2009 Luca Dionisi <luca.dionisi@gmail.com>
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

import os
import re
import subprocess

def get_netid():
    try:
        f = open("/tmp/netid.log")
    except:
        return None
    try:
        try:
            f.seek(-20, os.SEEK_END)
        except:
            pass # fewer bytes.
        lines = f.readlines()
        # last line, without \n
        l = lines[-1][:-1]
        if l[0] != '-':
            # group 3 digits
            tot = ''
            while len(l) > 3:
                tot = ',' + l[-3:] + tot
                l = l[:-3]
            l = l + tot
        return l
    except:
        return None
    finally:
        f.close()

def get_zombie():
    try:
        f = open("/tmp/zombie.log")
    except:
        return None
    try:
        try:
            f.seek(-20, os.SEEK_END)
        except:
            pass # fewer bytes.
        lines = f.readlines()
        # last line, without \n
        l = lines[-1][:-1]
        return l
    except:
        return None
    finally:
        f.close()

def get_ipaddr():
    proc = subprocess.Popen(['/sbin/ip', 'addr', 'show', 'eth0'],
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()
    r = re.compile(r'''inet\s((?:\d{1,3}\.){3}\d{1,3})/''')
    matched_address = r.search(stdout_value)
    return matched_address.groups()[0] if matched_address else ''

def get_neighbours_routes():
    neighbours = []
    routes = []
    proc = subprocess.Popen(['/sbin/ip', 'route'],
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            close_fds=True)
    stdout_value, stderr_value = proc.communicate()
    lines = stdout_value.split('\n')[:-1]
    for l in lines:
        if l.find(' via ') == -1 and l.find(' src ') == -1:
            # neigh
            neighbours.append(l[:l.find(' ')])
        else:
            # route
            routes.append(l[:l.find(' ')])
    return neighbours, routes


import curses
class pippo(object):
    def __init__(self):
        self.A_REVERSE = None
    def addstr(self,a,b,s,c=None):
        print s
    def wrapper(self,f):
        f(self)
    def refresh(self):
        pass
#curses = pippo()

import time

def main(stdscr):
    curses.init_pair(1,curses.COLOR_BLACK,curses.COLOR_WHITE)
    attr_normal = curses.color_pair(1)
    curses.init_pair(2,curses.COLOR_BLUE,curses.COLOR_WHITE)
    attr_title = curses.color_pair(2)
    stdscr.bkgd(ord(' '),attr_normal)

    oldnetid = ''
    oldzombie = ''

    while True:
        netid = get_netid()
        if netid is None:
            netid = '?' + oldnetid
        else:
            oldnetid = netid
        zombie = get_zombie()
        if zombie is None:
            zombie = '?' + oldzombie
        else:
            oldzombie = zombie

        stdscr.clear()

        stdscr.addstr(0, 0, "NETID", attr_title)
        stdscr.addstr(1, 0, netid)

        stdscr.addstr(2, 0, "My IP", attr_title)
        stdscr.addstr(3, 0, get_ipaddr())

        stdscr.addstr(4, 0, "ZOMBIE", attr_title)
        stdscr.addstr(5, 0, zombie)

        neighbours, routes = get_neighbours_routes()

        stdscr.addstr(0, 17, "Neighbours", attr_title)
        i = 1
        for n in neighbours:
            stdscr.addstr(i, 17, n)
            i += 1

        stdscr.addstr(0, 34, "Routes", attr_title)
        i = 1
        for r in routes:
            stdscr.addstr(i, 34, r)
            i += 1

        stdscr.refresh()
        time.sleep(4)

curses.wrapper(main)

