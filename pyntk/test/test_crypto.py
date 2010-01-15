##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
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

sys.path.append('..')

from ntk.lib import crypto as crypto
from ntk.lib import rencode as rencode

import random
def randomstring():
    sz = random.randint(1024, 10240)
    mesg = ''
    for i in xrange(sz):
        mesg += chr(random.randint(0, 255))
    return mesg

class TestCrypto(unittest.TestCase):

    def setUp(self):
        self.temp_dir = os.getcwd() + "/keys"
        if not os.path.exists(self.temp_dir):
            os.mkdir(self.temp_dir)
        else:
            files = os.listdir(self.temp_dir)
            for f in files:
                os.remove(self.temp_dir + "/" + f)

    def tearDown(self):
        files = os.listdir(self.temp_dir)
        for f in files:
            os.remove(self.temp_dir + "/" + f)
        os.rmdir(self.temp_dir)

    def testGenerateKeys(self):
        kpair = crypto.KeyPair()

    def testSaveAndLoadKeys(self):
        kpair = crypto.KeyPair()
        kpair.save_pair(self.temp_dir + "/keypair")
        kpair2 = crypto.KeyPair(self.temp_dir + "/keypair")
        self.failUnlessEqual(kpair.get_pub_key(), kpair2.get_pub_key())

    def testSaveAndLoadPKs(self):
        kpair = crypto.KeyPair()
        kpair.save_pub_key(self.temp_dir + "/pk")
        pk = crypto.PublicKey(from_file=(self.temp_dir + "/pk"))
        self.failUnlessEqual(kpair.get_pub_key(), pk)

    def testSignVerify(self):
        kpair = crypto.KeyPair()
        kpair.save_pub_key(self.temp_dir + "/pk")
        pk = crypto.PublicKey(from_file=(self.temp_dir + "/pk"))
        mesg = randomstring()
        signature = kpair.sign(mesg)
        self.failUnless(pk.verify(mesg, signature))
        self.failIf(pk.verify('another message', signature))

    def testEncDec(self):
        kpair = crypto.KeyPair()
        kpair.save_pub_key(self.temp_dir + "/pk")
        pk = crypto.PublicKey(from_file=(self.temp_dir + "/pk"))
        mesg = randomstring()
        self.failUnlessEqual(kpair.decrypt(pk.encrypt(mesg)), mesg)

    def testPack(self):
        pk1 = crypto.KeyPair().get_pub_key()
        args = pk1._pack()
        args = args[1:]
        pk2 = crypto.PublicKey(*args)
        self.failUnlessEqual(pk1, pk2)
        self.failUnless(pk1 == pk2)
        self.failIf(pk1 != pk2)

    def testPKHash(self):
        pk1 = crypto.KeyPair().get_pub_key()
        pk2 = crypto.KeyPair().get_pub_key()
        pk3 = crypto.KeyPair().get_pub_key()
        pk4 = crypto.KeyPair().get_pub_key()
        pk5 = crypto.KeyPair().get_pub_key()
        pk6 = crypto.KeyPair().get_pub_key()
        pk7 = crypto.KeyPair().get_pub_key()
        a = {}
        a[pk1] = 1
        a[pk2] = 2
        a[pk3] = 3
        a[pk4] = 4
        a[pk5] = 5
        a[pk6] = 6
        a[pk7] = 7
        self.failUnlessEqual(a[pk1], 1)
        self.failUnlessEqual(a[pk2], 2)
        self.failUnlessEqual(a[pk3], 3)
        self.failUnlessEqual(a[pk4], 4)
        self.failUnlessEqual(a[pk5], 5)
        self.failUnlessEqual(a[pk6], 6)
        self.failUnlessEqual(a[pk7], 7)

if __name__ == '__main__':
    unittest.main()
