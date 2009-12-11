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
import curses
import time

def get_links():
    try:
        f = open("/tmp/monitor_links.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_etp():
    try:
        f = open("/tmp/monitor_etp.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_dropped():
    try:
        f = open("/tmp/monitor_dropped.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_paths():
    try:
        f = open("/tmp/monitor_paths.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_collisions():
    try:
        f = open("/tmp/monitor_collisions.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_splits():
    try:
        f = open("/tmp/monitor_splits.log")
    except:
        return None
    try:
        try:
            f.seek(-30, os.SEEK_END)
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

def get_status():
    try:
        f = open("/tmp/status.log")
    except:
        return None
    try:
        try:
            f.seek(-200, os.SEEK_END)
        except:
            pass # fewer bytes.
        lines = f.readlines()
        # last <n> lines, without \n
        l = []
        for i in xrange(4):
            l.append(lines[i-4][:-1])
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
        status = get_status()
        if status is None:
            status = ['-'] * 4
        links = get_links()
        if links is None:
            links = '-'
        etp = get_etp()
        if etp is None:
            etp = '-'
        dropped = get_dropped()
        if dropped is None:
            dropped = '-'
        paths = get_paths()
        if paths is None:
            paths = '-'
        collisions = get_collisions()
        if collisions is None:
            collisions = '-'
        splits = get_splits()
        if splits is None:
            splits = '-'

        stdscr.clear()

        stdscr.addstr(0, 0, "NETID", attr_title)
        stdscr.addstr(1, 0, netid)

        stdscr.addstr(2, 0, "My IP", attr_title)
        stdscr.addstr(3, 0, get_ipaddr())

        stdscr.addstr(4, 0, "ZOMBIE", attr_title)
        stdscr.addstr(5, 0, zombie)

        stdscr.addstr(6, 0, "STATUS", attr_title)
        stdscr.addstr(7, 0, status[0])
        stdscr.addstr(8, 0, status[1])
        stdscr.addstr(9, 0, status[2])
        stdscr.addstr(10, 0, status[3])

        stdscr.addstr(12, 0, "LINKS", attr_title)
        stdscr.addstr(12, 11, links)
        stdscr.addstr(13, 0, "ETP", attr_title)
        stdscr.addstr(13, 11, etp)
        stdscr.addstr(14, 0, "DROPPED", attr_title)
        stdscr.addstr(14, 11, dropped)
        stdscr.addstr(15, 0, "PATHS", attr_title)
        stdscr.addstr(15, 11, paths)
        stdscr.addstr(16, 0, "COLLISIONS", attr_title)
        stdscr.addstr(16, 11, collisions)
        stdscr.addstr(17, 0, "SPLITS", attr_title)
        stdscr.addstr(17, 11, splits)

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

