#!/usr/bin/env python
##
# This file is part of Netsukuku
# (c) Copyright 2009 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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

from ntk.lib.log import logger as logging
from ntk.lib.micro import microfunc
import ntk.wrap.xtime as xtime

num_link_new = 0
num_link_changed = 0
num_link_dead = 0
num_etp_in = 0
num_etp_out = 0
num_etp_drop_hooking = 0
num_etp_drop_hooked_waiting = 0
num_etp_drop_neigh_not_found = 0
num_etp_drop_changed = 0
num_etp_drop_sender_hooking = 0
num_path_info_received = 0
num_path_info_used = 0
num_path_info_sent = 0
num_collision_detected = 0
num_collision_ignored_hooking = 0
num_collision_rehook = 0
num_split_detected_rehook = 0

@microfunc(True)
def monitor_status():
    logging.info('monitor tasklet started.')
    links_old = ''
    etps_old = ''
    dropped_old = ''
    paths_old = ''
    collisions_old = ''
    splits_old = ''
    while True:
        links = str(num_link_new) + '/' + str(num_link_changed) + '/' + str(num_link_dead)
        if links != links_old:
            links_old = links
            logging.log_on_file('/tmp/monitor_links.log', links)
        etps = str(num_etp_in) + '/' + str(num_etp_out)
        if etps != etps_old:
            etps_old = etps
            logging.log_on_file('/tmp/monitor_etp.log', etps)
        dropped = str(num_etp_drop_hooking) + '/' + str(num_etp_drop_hooked_waiting) + \
                '/' + str(num_etp_drop_neigh_not_found) + '/' + str(num_etp_drop_changed) + \
                '/' + str(num_etp_drop_sender_hooking)
        if dropped != dropped_old:
            dropped_old = dropped
            logging.log_on_file('/tmp/monitor_dropped.log', dropped)
        paths = str(num_path_info_received) + '/' + str(num_path_info_used) + \
                '/' + str(num_path_info_sent)
        if paths != paths_old:
            paths_old = paths
            logging.log_on_file('/tmp/monitor_paths.log', paths)
        collisions = str(num_collision_detected) + '/' + str(num_collision_ignored_hooking) + \
                '/' + str(num_collision_rehook)
        if collisions != collisions_old:
            collisions_old = collisions
            logging.log_on_file('/tmp/monitor_collisions.log', collisions)
        splits = str(num_split_detected_rehook)
        if splits != splits_old:
            splits_old = splits
            logging.log_on_file('/tmp/monitor_splits.log', splits)
        xtime.swait(50)

monitor_status()

def link_new():
    global num_link_new
    num_link_new += 1

def link_changed():
    global num_link_changed
    num_link_changed += 1

def link_dead():
    global num_link_dead
    num_link_dead += 1

def etp_in():
    global num_etp_in
    num_etp_in += 1

def etp_out():
    global num_etp_out
    num_etp_out += 1

def etp_drop_hooking():
    global num_etp_drop_hooking
    num_etp_drop_hooking += 1

def etp_drop_hooked_waiting():
    global num_etp_drop_hooked_waiting
    num_etp_drop_hooked_waiting += 1

def etp_drop_neigh_not_found():
    global num_etp_drop_neigh_not_found
    num_etp_drop_neigh_not_found += 1

def etp_drop_changed():
    global num_etp_drop_changed
    num_etp_drop_changed += 1

def etp_drop_sender_hooking():
    global num_etp_drop_sender_hooking
    num_etp_drop_sender_hooking += 1

def path_info_received():
    global num_path_info_received
    num_path_info_received += 1

def path_info_used():
    global num_path_info_used
    num_path_info_used += 1

def path_info_sent():
    global num_path_info_sent
    num_path_info_sent += 1

def collision_detected():
    global num_collision_detected
    num_collision_detected += 1

def collision_ignored_hooking():
    global num_collision_ignored_hooking
    num_collision_ignored_hooking += 1

def collision_rehook():
    global num_collision_rehook
    num_collision_rehook += 1

def split_detected_rehook():
    global num_split_detected_rehook
    num_split_detected_rehook += 1

