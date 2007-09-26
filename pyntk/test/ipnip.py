import sys
sys.path.append("..")
from ntk.core.map import Map

class Fake:
        def __init__(self, lvl, id):pass

m=Map(4, 256, Fake, [0,0,0,0])
nip=[11,22,33,44]
print nip
ip=m.nip_to_ip(nip)
print ip
mnip = m.ip_to_nip(ip)
print mnip
assert nip == mnip
