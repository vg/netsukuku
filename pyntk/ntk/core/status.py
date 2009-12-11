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

class ZombieException(Exception): pass

class StatusManager(object):

    def __init__(self):
        self._gonna_hook = False
        self._hooking = False
        self._hooked_waiting_id = 0
        self._zombie_id = 0

    def _get_gonna_hook(self):
        return self._gonna_hook

    def _set_gonna_hook(self, value):
        self._gonna_hook = value
        if value:
            self._hooking = False
            self._hooked_waiting_id = 0

    gonna_hook = property(_get_gonna_hook, _set_gonna_hook)

    def _get_hooking(self):
        return self._hooking

    def _set_hooking(self, value):
        self._hooking = value
        if value:
            self._gonna_hook = False

    hooking = property(_get_hooking, _set_hooking)

    def _get_hooked_waiting(self):
        return self._hooked_waiting_id != 0

    hooked_waiting = property(_get_hooked_waiting)

    def set_hooked_waiting_id(self, value):
        if self._hooked_waiting_id != 0:
            raise Exception('StatusManager: Already in hooked_waiting')
        if value == 0:
            raise Exception('StatusManager: You have to generate a random ' \
                           + 'id or use unset_hooked_waiting_id')
        self._hooked_waiting_id = value
        self._hooking = False

    def unset_hooked_waiting_id(self, value):
        if self._hooked_waiting_id == 0:
            # It was already unset
            return
        if value != self._hooked_waiting_id:
            raise Exception('StatusManager: You are not the owner of this ' \
                           + 'hooked_waiting_id')
        self._hooked_waiting_id = 0

    def _get_zombie(self):
        return self._zombie_id != 0

    zombie = property(_get_zombie)

    def set_zombie_id(self, value):
        if self._zombie_id != 0:
            raise Exception('StatusManager: Already in zombie')
        if value == 0:
            raise Exception('StatusManager: You have to generate a random ' \
                           + 'id or use unset_zombie_id')
        # now I am a zombie:
        logging.log_on_file('/tmp/zombie.log', 1)
        self._zombie_id = value
        self._gonna_hook = False
        self._hooking = False
        self._hooked_waiting_id = 0

    def unset_zombie_id(self, value):
        if self._zombie_id == 0:
            # It was already unset
            return
        if value != self._zombie_id:
            raise Exception('StatusManager: You are not the owner of this ' \
                           + 'zombie_id')
        logging.log_on_file('/tmp/zombie.log', 0)
        self._zombie_id = 0

    def _get_hooked(self):
        return not (self._gonna_hook or \
                    self._hooking or \
                    self._hooked_waiting_id or \
                    self._zombie_id)

    hooked = property(_get_hooked)

