import os
import sys
import unittest
import time

# taken the same module skeleton from lukisi's code (test_dnswrapper.py)

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
from ntk.core.andnsserver import AndnsServer
from ntk.core.snsd import SnsdResolvedRecord, AndnaResolvedRecord

class FakeAndna(object):

    def resolve(self, hostname, serv_key, no_chain=False):
        records = [SnsdResolvedRecord('167.167.167.167', 10, 4),
                   SnsdResolvedRecord('164.164.164.164', 10, 4)]
        return AndnaResolvedRecord(100, records)

class FakeCounter(object):

    def ask_reverse_resolution(self, nip):
        records = [SnsdResolvedRecord('test.iwf.fi', 10, 4),
                   SnsdResolvedRecord('another.iwf.fi', 10, 4)]
        return AndnaResolvedRecord(100, records)

class TestAndnsServer(unittest.TestCase):

    def test_andns(self):
        AndnsServer(FakeAndna(), FakeCounter())
        # TODO
        micro(self.mf_test_andns)
        allmicro_run()

    def mf_test_andns(self):
        while True:
            time_swait(2000)
            ntk.lib.log.logger.debug('ciao')

if __name__ == '__main__':
    unittest.main()

