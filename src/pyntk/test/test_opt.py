import sys
sys.path.append('..')

from lib.opt import Opt

opt = Opt()
opt.opt_load_file("test_opt.conf")
print dir(opt)
