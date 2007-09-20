import sys

from socket import AF_INET, SOCK_DGRAM, SOCK_STREAM, SO_REUSEADDR, SOL_SOCKET
import traceback
import md5
import random

sys.path.append('../../')
from lib.micro import micro, microfunc, allmicro_run
sys.path.remove('../../'); sys.path.append('../')
from net import Net
import sim
from network.vsock import VirtualSocket

random.seed(1)

N=Net()

# load from file
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

import pdb

@microfunc()
def echo_srv():
    """Standard echo server example"""
    host = ''
    port = 51423
    s0=VirtualSocket(AF_INET, SOCK_DGRAM, N, N.net[0])
    s0.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s0.bind((host, port))
    try:
        message, address = s0.recvfrom(8192)
	print "Got data from %s: %s"%(address, message)
        s0.sendto('yo there', address)
    except:
        traceback.print_exc()

@microfunc()
def echo_client():
    s1=VirtualSocket(AF_INET, SOCK_DGRAM, N, N.net[1])
    ip = s1.inet.ip_to_str(0)
    print "sending data to "+ip
    s1.sendto('hey there', (ip, 51423))
    message, address = s1.recvfrom(8192)
    print "got reply from %s: %s"%(address, message)

sim.sim_activate()
echo_srv()
echo_client()
allmicro_run()
sim.sim_run()
