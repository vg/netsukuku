##
# This file is part of Netsukuku
# (c) Copyright 2007 Francesco Losciale aka jnz <frengo@anche.no>
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
 
from M2Crypto.RSA import RSA, gen_key, load_pub_key

import hashlib

 
def sha1(msg):
    s = hashlib.sha1()
    s.update(msg)
    return s.digest()
 
def md5(msg):
    m = hashlib.md5()
    m.update(msg)
    return m.digest()
 
def genkeys(keysize=1024, rsa_pub_exponent=65537):
    """ Generate a key pair
 
        @return M2Crypto.RSA.RSA 
    """
    def do_nothing: pass
    return gen_key(keysize, rsa_pub_exponent, callback=do_nothing) 
 
def get_pub_key(keys):
        '''Return just the public key'''
        # TODO: maybe just "return keys.pub" ?
        pass

def sign(key, msg):
    """ Sign a message with key
 
        @param key istance of M2Crypto.RSA.RSA
        @param msg
 
        @return signature 
    """
    return keys.sign(sha1(msg))
 
def verify(key, msg, signature): 
    """ Verify a message signature with key
 
        @param key istance of M2Crypto.RSA.RSA
        @param msg
        @param signature
 
        @return boolean
    """
    return keys.verify(sha1(msg), signature)
 

def save_keys(file):
        '''Save the keys to a file'''
        pass
def load_keys(file):
        pass
