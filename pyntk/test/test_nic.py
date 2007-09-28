import sys
sys.path.append('..')
from ntk.network.nic import Nic, NicAll

n=Nic("dummy0")
n.up()
print n.retrieve_info(4)
n.down()
print n.retrieve_info(4)
n.change_address("11.22.33.44")
print n.retrieve_info(4)
n.activate("11.22.33.55")
print n.retrieve_info(4)

na=NicAll()
print na.retrieve_info(4)
na=NicAll(nics=['dummy0', 'eth0'])
print na.retrieve_info(4)
