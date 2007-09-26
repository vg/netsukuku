import sys

import md5
import random
from random import randint
import pdb


sys.path[0]='..'
from net import Net

random.seed(1)

# load from file
N=Net()
N.net_file_load('net1')
N.net_dot_dump(open('net1.dot', 'w'))

# rand net
N1=Net()
N1.rand_net_build(16)
N1.net_dot_dump(open('net1-rand.dot', 'w'))

# mesh net
N2=Net()
N2.mesh_net_build(4)
N2.net_dot_dump(open('net1-mesh.dot', 'w'))

# complete net
N3=Net()
N3.complete_net_build(8)
N3.net_dot_dump(open('net1-complete.dot', 'w'))

m=md5.new(); m.update(open('net1-rand.dot', 'r').read())
assert m.hexdigest() == 'f7139ff18ac064c619588e4e02002ceb'

m=md5.new(); m.update(open('net1-mesh.dot', 'r').read())
assert m.hexdigest() == '9aa64c45eaba5af3df99f9919f50fba3'

m=md5.new(); m.update(open('net1-complete.dot', 'r').read())
assert m.hexdigest() == '8ba964a2d421e4a4d86980daffc290c5'

m=md5.new(); m.update(open('net1.dot', 'r').read())
assert m.hexdigest() == '7665a14c07f50187b2c7844643d0bb77'
