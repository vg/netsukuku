##
# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
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
#
# Tests for ntk.core.map
#

import unittest

import sys
sys.path.append('..')

# for this test all IP are valid
from ntk.network.inet import valid_ids
def f(lvl,nip):
    return xrange(256)
valid_ids = f

from ntk.core.map import Map, DataClass
from ntk.lib.rencode import serializable, loads, dumps
from ntk.lib.micro import microfunc
from random import randint, choice
from ntk.lib.log import logger as logging
logging.ULTRADEBUG=5

# This is a serializable Node. It is used as dataclass.
# The node has an attribute (attr1) that indicates if the node has a route.
# Normally a node without a route is free; but there is an exception for the
# node representing myself. It has to be not-free, but it has to have no route.
# The node has got also another attribute (attr2) just to test serialization.
class NodeSerializable(object):
    def __init__(self, lvl, id, attr1=None, attr2=None, its_me=False):
        self.lvl = lvl
        self.id = id
        self.attr1 = attr1
        self.attr2 = attr2
        self.its_me = its_me

    def is_free(self):
        if self.its_me: return False
        if self.attr1: return False
        return True

    def get_routes(self):
        return self.attr1

    def _pack(self):
        return (self.lvl, self.id, self.attr1, self.attr2)

serializable.register(NodeSerializable)

class DerivMap(Map):
    def deriv_node_add(self, lvl, dst):
        node = self.node_get(lvl, dst)
        if node.is_free():
            self.node_get(lvl, dst).attr1 = 'route'
            self.node_add(lvl, dst)
    def deriv_node_del(self, lvl, dst):
        self.node_del(lvl, dst)


class TestMap(unittest.TestCase):

    def setUp(self):
        self.map_levels = 4
        self.map_gsize  = 256
        self.map_me = [89, 67, 45, 123]

    def get_random_nip(self):
        nip = []
        while len(nip) < self.map_levels:
            nip.append(randint(0, self.map_gsize-1))
        return nip

    def randomize_nips(self, number, func, args=(), nip=[], exclude_nips=[]):
        """Call a function for some NIPs.
        
        Choose the NIP randomly. You may obtain the same NIP more than once.
        You can specify a number of NIPs to be excluded."""
        for i in xrange(number):
            nip = None
            while True:
                nip = self.get_random_nip()
                if nip not in exclude_nips: break
            func(nip, *args)

    def randomize_unique_nips(self, number, func, args=(), nip=[], exclude_nips=[]):
        """Call a function for some NIPs.
        
        Choose the NIP randomly. You may *not* obtain the same NIP more than once.
        If the requested number of NIPs is near to the maximum available, this function won't perform well.
        You can specify a number of NIPs to be excluded."""
        nips = []
        for i in xrange(number):
            nip = None
            while True:
                nip = self.get_random_nip()
                if nip not in nips+exclude_nips: break
            nips.append(nip)
            func(nip, *args)

    def all_nips(self, func, args=(), nip=[], exclude_nips=[]):
        """Call a function for all the available NIPs.
        
        You can specify a number of NIPs to be excluded."""
        if len(nip) == self.map_levels:
            # we have a nip
            if nip not in exclude_nips:
                func(nip, *args)
        else:
            for i in xrange(self.map_gsize):
                nip2 = nip[:]
                nip2.append(i)
                self.all_nips(func, args, nip2, exclude_nips=exclude_nips)

    def test01MapCreation(self):
        '''Test Map Creation'''
        self.mymap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, self.map_me)
        if self.mymap.me is self.map_me:
            self.fail('Una mappa deve farsi una copia dell\'indirizzo passato come "me"')
        if self.mymap.me != self.map_me:
            self.fail('Una mappa deve farsi una copia *esatta* dell\'indirizzo passato come "me"')
        if self.mymap.levels != self.map_levels:
            self.fail('Una mappa deve memorizzare il numero di livello passato')
        if self.mymap.gsize != self.map_gsize:
            self.fail('Una mappa deve memorizzare il numero di nodi per livello passato')
        for lvl in xrange(self.map_levels):
            if self.mymap.node_nb[lvl] != 1:
                self.fail('Una mappa appena creata dovrebbe avere 1 posto occupato per ogni livello')
            node = self.mymap.node_get(lvl, self.mymap.me[lvl])
            if node.is_free():
                self.fail('La mappa non vede come occupato il posto di "me".')

    def ListeningNodeNew(self, lvl, id):
        if self.mymap.node_get(lvl, id).is_free():
            self.fail('Sul listener dell\'evento NODE_NEW devo avere il nodo occupato.')

    @microfunc(True)
    def MicroListeningNodeNew(self, lvl, id):
        if self.mymap.node_get(lvl, id).is_free():
            self.fail('Sul listener *microfunc* dell\'evento NODE_NEW devo avere il nodo occupato.')

    def test02MapNodeAdd(self):
        '''Test Add Node to Map'''
        self.mymap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, self.map_me)
        self.mymap.events.listen('NODE_NEW', self.ListeningNodeNew)
        self.mymap.events.listen('NODE_NEW', self.MicroListeningNodeNew)
        busynodes = self.map_levels  # 'me' occupa un posto su ogni livello
        nip3 = [1,2,3,4]
        nip2 = [1,2,3,123]
        nip2again = [2,2,3,123]
        nip1 = [1,2,45,123]
        nip0 = [1,67,45,123]
        nips = [(nip3, True), (nip2, True), (nip2again, False), (nip1, True), (nip0, True)]
        for nip, inc in nips:
            lev = self.mymap.nip_cmp(self.mymap.me, nip)
            self.mymap.deriv_node_add(lev, nip[lev])
            if inc: busynodes += 1   # Should this go in a new (g)node?
            if sum(self.mymap.node_nb) != busynodes:
                self.fail('Aggiungere un nodo deve incrementare di 1 i posti occupati')

    def ListeningNodeDeleted(self, lvl, id):
        if not self.mymap.node_get(lvl, id).is_free():
            self.fail('Sul listener dell\'evento NODE_DELETED devo avere il nodo libero.')

    @microfunc(True)
    def MicroListeningNodeDeleted(self, lvl, id):
        if not self.mymap.node_get(lvl, id).is_free():
            self.fail('Sul listener *microfunc* dell\'evento NODE_DELETED devo avere il nodo libero.')

    def test03MapNodeDel(self):
        '''Test Del Node from Map'''
        self.mymap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, self.map_me)
        self.mymap.events.listen('NODE_DELETED', self.ListeningNodeDeleted)
        self.mymap.events.listen('NODE_DELETED', self.MicroListeningNodeDeleted)
        busynodes = self.map_levels  # 'me' occupa un posto su ogni livello
        nip = [1,2,3,4]
        lev = self.mymap.nip_cmp(self.mymap.me, nip)
        self.mymap.deriv_node_add(lev, nip[lev])
        nip = [1,2,3,123]
        lev = self.mymap.nip_cmp(self.mymap.me, nip)
        self.mymap.deriv_node_add(lev, nip[lev])
        pair_to_delete = (lev, nip[lev])
        busynodes = sum(self.mymap.node_nb)
        self.mymap.deriv_node_del(*pair_to_delete)
        if sum(self.mymap.node_nb) != busynodes-1:   # un nodo cancellato libera un posto
            self.fail('Cancellare un nodo deve decrementare di 1 i posti occupati')

    def ListeningTwiceNodeNew(self, lvl, id):
        self.fail('L\'evento NODE_NEW deve avvenire solo una volta su un nodo.')

    def ListeningTwiceNodeDeleted(self, lvl, id):
        self.fail('L\'evento NODE_DELETED deve avvenire solo una volta su un nodo.')

    def test04MapEventOnlyOnce(self):
        '''Test that, with a correct implementation of the derived Map, the event NODE_NEW and NODE_DELETED
           are raised at most once.'''
        nip = [1,2,3,4]
        self.mymap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, self.map_me)
        lev = self.mymap.nip_cmp(nip, nip)
        self.mymap.deriv_node_add(lev, nip[lev])
        self.mymap.events.listen('NODE_NEW', self.ListeningTwiceNodeNew)
        self.mymap.deriv_node_add(lev, nip[lev])
        self.mymap.deriv_node_del(lev, nip[lev])
        self.mymap.events.listen('NODE_DELETED', self.ListeningTwiceNodeNew)
        self.mymap.deriv_node_del(lev, nip[lev])

    def test05MapSerializing(self):
        '''Test Serialization of Map'''
        def f(node):
            node.attr1 = 'route'
        self.mymap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, self.map_me)
        nip = [1,2,3,4]
        lev = self.mymap.nip_cmp(self.mymap.me, nip)
        self.mymap.deriv_node_add(lev, nip[lev])
        nip = [1,2,3,123]
        lev = self.mymap.nip_cmp(self.mymap.me, nip)
        self.mymap.deriv_node_add(lev, nip[lev])
        packmymap = self.mymap.map_data_pack(f)
        sermymap = dumps(packmymap)
        if dumps(loads(sermymap)) != sermymap:
            self.fail('Errore nella dumps(loads()) della mappa (serializzazione inversa)')
        anothermap = DerivMap(self.map_levels, self.map_gsize, NodeSerializable, [1,2,3,123])
        anothermap.map_data_merge(loads(sermymap))
        if anothermap.node_get(3, 123).is_free() or \
           anothermap.node_get(2, 3).is_free() or \
           anothermap.node_get(1, 2).is_free() or \
           anothermap.node_get(0, 1).is_free() or \
           not anothermap.node_get(3, 123).its_me or \
           not anothermap.node_get(2, 3).its_me or \
           not anothermap.node_get(1, 2).its_me or \
           not anothermap.node_get(0, 1).its_me or \
           anothermap.node_get(3, 123).attr1 is not None or \
           anothermap.node_get(2, 3).attr1 is not None or \
           anothermap.node_get(1, 2).attr1 is not None or \
           anothermap.node_get(0, 1).attr1 is not None or \
           False:
            self.fail('Serializzazione + merge: il (g)nodo me (a qualche livello) non e\' valorizzato correttamente.')
        if anothermap.node_get(3, 4).is_free() or \
           anothermap.node_get(3, 4).its_me or \
           anothermap.node_get(3, 4).attr1 is None or \
           False:
            self.fail('Serializzazione + merge: il gnodo livello 3 id 4 non e\' valorizzato correttamente.')
        if not anothermap.node_get(1, 1).is_free(): # node unknown
            self.fail('Serializzazione + merge: il gnodo livello 1 id 1 non e\' valorizzato correttamente.')
        if anothermap.node_get(2, 45).is_free() or \
           anothermap.node_get(2, 45).its_me or \
           anothermap.node_get(2, 45).attr1 is None or \
           False:
            self.fail('Serializzazione + merge: il gnodo livello 2 id 45 non e\' valorizzato correttamente.')

if __name__ == '__main__':
    unittest.main()

