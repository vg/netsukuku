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

import hashlib
import os

from M2Crypto import version_info
from M2Crypto.BIO import openfile, File, MemoryBuffer
from M2Crypto.RSA import (RSA, RSAError, gen_key, load_key, load_key_bio,
     load_pub_key, load_pub_key_bio, load_key_string)

from ntk.config import settings, ImproperlyConfigured
from ntk.lib.rencode import serializable

if version_info < (0, 19):
    raise ImproperlyConfigured('M2Crypto >= 0.19 is required')

##
# Simple hash functions
##

# Fowler-Noll-Vo hash function
def fnv_32_buf(msg, hval=0x811c9dc5):
    for c in xrange(len(msg)):
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24)
        hval ^= ord(msg[c])
    return hval

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

def do_nothing(null): return "passphrase"

class CryptoError(Exception): pass

def verify(msg, signature, pubk):
    """ Verify a message signature with the given public key """
    try:
        return pubk.verify(msg, signature)
    except RSAError:
        return False

def public_key_from_pem(pem_string):
    return PublicKey(pem_string)

class PublicKey():

    def __init__(self, rsa=None, pem_string=None):
        if rsa is None and pem_string is None:
            raise CryptoError("Invalid parameter passed to the constructor")
        elif rsa is None:
            self.rsa = load_pub_key_bio(MemoryBuffer(pem_string))
        elif pem_string is None:
            self.rsa = rsa

    def __getattr__(self, name):
        return getattr(self.rsa, name)

    def verify(self, msg, signature):
        try:
            return self.rsa.verify(msg, signature)
        except RSAError:
            return False

    def __repr__(self):
        return self.rsa.as_pem()

    def _pack(self):
        return (self.rsa.as_pem(),)

serializable.register(PublicKey)

class KeyPair():

    def __init__(self, keys_path=settings.KEY_PAIR_DIR):
        if os.path.exists(keys_path) and os.path.getsize(keys_path) > 0:
            self.bio = openfile(keys_path, 'rb')
            self.rsa = load_key_bio(self.bio, callback=do_nothing)
        else:
            self.bio = openfile(keys_path, 'ab+')
            self.rsa = self.generate()
            self.rsa.save_key_bio(self.bio, callback=do_nothing)
            self.rsa.save_pub_key_bio(self.bio)
        self.bio.reset()

    def generate(self, keysize=1024, rsa_pub_exponent=65537):
        """ Generate a key pair """
        return gen_key(keysize, rsa_pub_exponent, callback=do_nothing)

    def get_pub_key(self):
        """ Return just the public key (RSA.RSA_pub) """
        rsa_pubk = load_pub_key_bio(self.bio)
        self.bio.reset()
        return PublicKey(rsa=rsa_pubk)

    def sign(self, msg):
        """ Sign the message with my private key """
        return self.rsa.sign(msg)

    def verify(self, msg, signature):
        """ Verify a message signature with my public key """
        try:
            return self.get_pub_key().verify(msg, signature)
        except RSAError:
            return False

    def save(self):
        """ Save the keys to a file """
        self.rsa.save_key_bio(self.bio, callback=do_nothing)
        self.rsa.save_pub_key_bio(self.bio)
        self.bio.reset()

    def load(self):
        """ Load keys from file """
        load_key_bio(self.bio, callback=do_nothing)
        self.bio.reset()
