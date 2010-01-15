# -*- coding: utf-8 -*-
##
# This file is part of Netsukuku
# (c) Copyright 2009 Francesco Losciale aka jnz <francesco.losciale@gmail.com>
# (c) Copyright 2010 Luca Dionisi aka lukisi <luca.dionisi@gmail.com>
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

import hashlib
import os

from M2Crypto import version_info

from M2Crypto import RSA, BIO

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.rencode import serializable

if version_info < (0, 19):
    raise ImproperlyConfigured('M2Crypto >= 0.19 is required')

# Non-cryptographic hash
def fnv_32_buf(msg, hval=0x811c9dc5):
    """ Fowler-Noll-Vo hash function """
    for c in xrange(len(msg)):
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24)
        hval ^= ord(msg[c])
    return hval

# Cryptographic hash
def sha1(msg):
    s = hashlib.sha1()
    s.update(msg)
    return s.digest()

def md5(msg):
    m = hashlib.md5()
    m.update(msg)
    return m.digest()

##
# RSA stuff
##

class CryptoError(Exception): pass

RSA_SIZE_BYTES = 128

class KeyPair(object):
    def __init__(self, from_file=None):
        # if None: generate_keypair
        if from_file is None:
            def void(): pass
            self.rsa = RSA.gen_key(RSA_SIZE_BYTES * 8, 65537, callback=void)
        else:
            self.rsa = RSA.load_key(from_file)

    def save_pair(self, keys_path):
        self.rsa.save_key(keys_path, cipher=None)

    def save_pub_key(self, pk_path):
        self.rsa.save_pub_key(pk_path)

    def get_pub_key(self):
        bio = BIO.MemoryBuffer()
        self.rsa.save_pub_key_bio(bio)
        pk = RSA.load_pub_key_bio(bio)
        return PublicKey(from_pem=pk.as_pem())

    def sign(self, msg):
        return self.rsa.sign(md5(msg))

    def decrypt(self, cipher):
        packetlen = RSA_SIZE_BYTES
        msg = ''
        while True:
            to_dec = cipher[:packetlen]
            cipher = cipher[packetlen:]
            msg += self.rsa.private_decrypt(to_dec, RSA.pkcs1_padding)
            if len(cipher) == 0:
                break
        return msg

    def __repr__(self):
        return '<KeyPair: (pubk ' + self.get_pub_key().short_repr() + ')>'

class PublicKey(object):
    def __init__(self, from_pem=None, from_file=None):
        # from_pem used when receiving a pubk in a SnsdRecord
        #        or when generating the pubk from our keypair.
        # from_file used when reading a pubk stored in a file
        # (no both None)
        if from_pem is not None:
            self.pk = RSA.load_pub_key_bio(BIO.MemoryBuffer(from_pem))
        elif from_file is not None:
            self.pk = RSA.load_pub_key(from_file)
        elif from_file is not None:
            raise CryptoError('Invalid parameter passed to the constructor')

    def save_pub_key(self, pk_path):
        self.pk.save_pub_key(pk_path)

    def verify(self, msg, signature):
        ret = False
        try:
            ret = self.pk.verify(md5(msg), signature)
            ret = True if ret else False
        except RSA.RSAError:
            pass
        return ret

    def encrypt(self, msg):
        maxlen = RSA_SIZE_BYTES - 11
        cipher = ''
        while True:
            to_cip = msg[:maxlen]
            msg = msg[maxlen:]
            cipher += self.pk.public_encrypt(to_cip, RSA.pkcs1_padding)
            if len(msg) == 0:
                break
        return cipher

    def __eq__(self, other):
        return self.pk.as_pem() == other.pk.as_pem()

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self.pk.as_pem())

    def _pack(self):
        return (self.pk.as_pem(), None)

    def short_repr(self):
        return '...' + str(self.pk.as_pem())[81:90]  + '...'

    def __str__(self):
        return self.short_repr()
        return '<PublicKey ' + self.short_repr()  + '>'

    def __repr__(self):
        return self.short_repr()
        return '<PublicKey ' + self.short_repr()  + '>'

serializable.register(PublicKey)

