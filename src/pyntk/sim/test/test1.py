import sys
sys.path.append('../../')
from lib.micro import micro, allmicro_run
sys.path.remove('../../')
sys.path.append('../')
from net import Net
from network.vsock import VirtualSocket

N=Net()

N.net_file_load('net1')
f=open('net1.dot', 'w')
N.net_dot_dump(f)
f.close()
