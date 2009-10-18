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


class TestCrypto(unittest.TestCase):

    def setUp(self):
        self.temp_dir = os.getcwd() + "/keys"
        self.msg  = "Message"
        if not os.path.exists(self.temp_dir):
            os.mkdir(self.temp_dir)
        else:
            files = os.listdir(self.temp_dir)
            for f in files:
                os.remove(self.temp_dir + "/" + f)
        self.keys = crypto.KeyPair(self.temp_dir + "/keypair")

    def tearDown(self):
        files = os.listdir(self.temp_dir)
        for f in files:
            os.remove(self.temp_dir + "/" + f)
        os.rmdir(self.temp_dir)

    def testGenerateKeys(self):
        self.failIfEqual(self.keys.rsa, None)

    def testSignVerify(self):
        self.signature = self.keys.sign(self.msg)
        result = self.keys.verify(self.msg, self.signature)
        self.failUnlessEqual(result, True)

    def testSaveKeys(self):
        self.keys.save()

    def testLoadKeys(self):
        self.keys.load()

    def testMessageValidity(self):
        # host 1
        self.keys_host1 = crypto.KeyPair(self.temp_dir + "/keypair1")
        # host 2
        self.keys_host2 = crypto.KeyPair(self.temp_dir + "/keypair2")

        # host 1 sign the message and send it to host 2, with
        # the attached signature and public key
        signature = self.keys_host1.sign(self.msg)
        host1_pub_key = self.keys_host1.get_pub_key()

        # host 2 receive the message with signature and public
        # key of host 1, and verify the authenticity of the sender
        result = crypto.verify(self.msg, signature, host1_pub_key)
        self.failUnlessEqual(result, True)

    def testMessageInvalidity(self):
        # host 1
        self.keys_host1 = crypto.KeyPair(self.temp_dir + "/key1")
        # host 2
        self.keys_host2 = crypto.KeyPair(self.temp_dir + "/key2")
        # host 3
        self.keys_host3 = crypto.KeyPair(self.temp_dir + "/key3")

        # host 1 sign the message and send it to host 2,
        # with the attached signature and public key
        signature = self.keys_host1.sign(self.msg)
        host1_pub_key = self.keys_host2.get_pub_key() # pass the wrong key

        # host 2 receive the message with signature and public key of host 1,
        # and verify the authenticity of the sender
        result = host1_pub_key.verify(self.msg, signature)
        result = crypto.verify(self.msg, signature, host1_pub_key)
        self.failUnlessEqual(result, False)
        result = crypto.verify(self.msg, signature,
                               self.keys_host1.get_pub_key())
        self.failUnlessEqual(result, True)

    def testFNV(self):
        self.failUnlessEqual(crypto.fnv_32_buf(self.msg),
        810584075510011066574254008878303710221824016571840399774474L)
        self.failUnlessEqual(crypto.fnv_32_buf(""), 2166136261L)

    def testPublicKeySerialization(self):
        keys = crypto.KeyPair(self.temp_dir + "/temp_key")
        signature = keys.sign("Message")
        pub_key = keys.get_pub_key()
        classname, pem_string = pub_key._pack()
        pub_key = crypto.public_key_from_pem(pem_string=pem_string)
        self.failUnlessEqual(crypto.verify("Message", signature, pub_key),
                             True)
        self.failUnlessEqual(rencode.loads(rencode.dumps(pub_key)),
                             pub_key)
        #print rencode.dumps(pub_key)
        #print rencode.loads(rencode.dumps(pub_key))
        #print pub_key.rsa.as_pem()

    def testHash(self):
        keys1 = crypto.KeyPair(self.temp_dir + "/temp_key1")
        keys2 = crypto.KeyPair(self.temp_dir + "/temp_key2")
        pubk1 = keys1.get_pub_key()
        pubk2 = keys2.get_pub_key()
        self.failUnlessEqual(pubk1.__hash__() == pubk2.__hash__(), False)
        self.failUnlessEqual(pubk1.__hash__() == pubk1.__hash__(), True)
        self.failUnlessEqual(pubk1 == pubk2, False)

if __name__ == '__main__':
    unittest.main()
