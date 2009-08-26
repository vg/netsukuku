import sys
import unittest
sys.path.append('..')

from ntk.lib.rencode import dumps, loads, serializable


class Node(object):
    def __init__(self, 
                 lvl=None, id=None, alive=False  # these are mandatory for Map.__init__(),
                ):
        
        self.alive = alive

    def is_free(self):
        return not self.alive

    def _pack(self):
        return (0, 0, self.alive)
    
    # needed only in this test (by failUnlessEqual)
    def __cmp__(self, b): 
	return cmp(self.alive, b.alive)
    	
serializable.register(Node)


class A(object):
    def __init__(self, a, b, c):
        self.a = a
        self.b = b
        self.c = c

    def _pack(self):
        return (self.a, self.b, self.c)

    def __cmp__(self, b):
	return cmp(self.a+self.b+self.c, b.a+b.b+b.c)

serializable.register(A)


class TestRencode(unittest.TestCase):
	
     def setUp(self): 
         self.node = Node(alive=True)
	 self.instance = A(1,2,3)
	 self.tuple = (self.instance, self.node, self.instance)
         self.list = [ 23, 'ffff', ('a',10**20), 'b'*64,2**30,2**33,
	               2**62,2**64, 2**30,2**33,2**62,2**64, self.instance, 
		       self.instance, self.node, self.tuple ]
	 self.dict = { 23: 'rrrr', 55: self.list , 66: self.instance }
	  	 
     def testNode(self):
	 encoded_node = dumps(self.node)
	 decoded_node = loads(encoded_node)
         self.failUnlessEqual(self.node.alive, decoded_node.alive)

     def testInstance(self):
         self.failUnlessEqual(loads(dumps(self.instance)), self.instance)

     def testTuple(self):
	 self.failUnlessEqual(loads(dumps(self.tuple)), self.tuple)
     
     def testList(self):
	 self.failUnlessEqual(loads(dumps(self.list)), self.list)

     def testDict(self):
	 self.failUnlessEqual(loads(dumps(self.dict)), self.dict)


if __name__ == '__main__':
    unittest.main()