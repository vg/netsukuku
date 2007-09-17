#!/usr/bin/env python
import sys
sys.path.append('..')

from lib.opt import Opt

opt = Opt({'s1':'short_opt_1', 's2':'short_opt_2'})
opt.load_file("test_opt.conf")
opt.load_argv(['./test_opt.py', 'argv_a=1; argv_b=2; s1=3; s2="test"', 'boolean_opt'])
print dir(opt)
assert opt.a == 1
assert opt.b == 2
assert opt.c == 4
assert opt.f == "asdasd"
assert opt.short_opt_1 == 3
assert opt.short_opt_2 == "test"
assert opt.boolean_opt == True
print opt.getdict()
print opt.getdict(['a', 'b', 'argv_a', 'nonexistentopt'])
