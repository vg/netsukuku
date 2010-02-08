##
# This file is part of Netsukuku
# (c) Copyright 2010 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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
import sys
import unittest
import time

sys.path.append('..')

log_on_console = True
log_container = []
def log_func(args):
    print args

class fakelogger(object):
    def log(self, *args): log_func(args[1])
    def debug(self, *args): log_func(args)
    def error(self, *args): log_func(args)
    def info(self, *args): log_func(args)
    def warning(self, *args): log_func(args)
class fakeexception(Exception): pass
def faketrace(*args): log_func(args)

import ntk.lib.log
ntk.lib.log.logger = fakelogger()
ntk.lib.log.logger.ULTRADEBUG=1
ntk.lib.log.log_exception_stacktrace = faketrace
ntk.lib.log.ExpectableException = fakeexception

from ntk.lib.micro import allmicro_run, micro, time_swait
from ntk.core.dnswrapper import DnsWrapper
from ntk.core.dnswrapper import AndnsServer as fakeandnsserver

class TestDnsWrapper(unittest.TestCase):

    def test_dns(self):
        DnsWrapper(None, fakeandnsserver(None))
        # TODO
        micro(self.mf_test_dns)
        allmicro_run()

    def mf_test_dns(self):
        while True:
            time_swait(2000)
            ntk.lib.log.logger.debug('ciao')

if __name__ == '__main__':
    unittest.main()

