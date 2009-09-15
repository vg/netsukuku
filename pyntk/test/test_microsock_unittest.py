# This file is part of Netsukuku
#
# Tests for ntk.lib.microsock

from socket import AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, SOCK_DGRAM

import sys
import unittest
sys.path.append('..')

from ntk.lib.micro import micro, microfunc, allmicro_run, Channel
from ntk.wrap.sock import Sock
import ntk.wrap.xtime as xtime
socket = Sock()
from ntk.lib.microsock import MicrosockTimeout
from ntk.network.inet import sk_bindtodevice, sk_set_broadcast

resultsTest = []

@microfunc(is_micro=True)
def serverAcceptWithTimeout(_timeout=None, _number=0):
    _name = ''
    if _number != 0:
        _name = ' ' + str(_number)
    listenSocket = socket.socket(AF_INET, SOCK_STREAM)
    listenSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    listenSocket.bind(("127.0.0.1", 3000 + _number))
    listenSocket.listen(5)
    try:
        if _timeout is None: currentSocket, clientAddress = listenSocket.accept()
        else: currentSocket, clientAddress = listenSocket.accept(timeout=_timeout)
    except MicrosockTimeout:
        resultsTest.append('Timed Out' + _name)
    else:
        resultsTest.append('Accepted' + _name)
    resultsTest.append('Exit Server Microfunc' + _name)

@microfunc(is_micro=True)
def clientAcceptWithTimeout(_delay=100, _number=0):
    xtime.swait(_delay)
    clientSocket = socket.socket()
    try:
        clientSocket.connect(("127.0.0.1", 3000 + _number))
    except Exception:
        pass #resultsTest.append('Connect Error')
    else:
        pass #resultsTest.append('Connected')

@microfunc(is_micro=True)
def serverRecvfromWithTimeout(_timeout=None):
    listenSocket = socket.socket(AF_INET, SOCK_DGRAM)
    listenSocket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    sk_bindtodevice(listenSocket, 'eth0')
    listenSocket.bind(("", 3000))
    try:
        if _timeout is None: message, address = listenSocket.recvfrom(8192)
        else: message, address = listenSocket.recvfrom(8192, timeout=_timeout)
    except MicrosockTimeout:
        resultsTest.append('Timed Out')
    else:
        resultsTest.append('Received')
    resultsTest.append('Exit Server Microfunc')

@microfunc(is_micro=True)
def clientRecvfromWithTimeout():
    xtime.swait(100)
    clientSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sk_bindtodevice(clientSocket, 'eth0')
    sk_set_broadcast(clientSocket)
    clientSocket.connect(('<broadcast>', 3000))
    try:
        clientSocket.sendto('hello', ('<broadcast>', 3000))
    except Exception:
        pass #resultsTest.append('Sendto Error')
    else:
        pass #resultsTest.append('Sent')


class TestMicrosock(unittest.TestCase):

    def setUp(self):
        pass

    def test01AcceptWithTimeoutExpired(self):
        '''Test expiring timeout in accept'''
        global resultsTest
        resultsTest = []
        clientAcceptWithTimeout()
        serverAcceptWithTimeout(30)
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Timed Out', 'Exit Server Microfunc'])

    def test02AcceptWithTimeout(self):
        '''Test timeout not expired in accept'''
        global resultsTest
        resultsTest = []
        clientAcceptWithTimeout()
        serverAcceptWithTimeout(200)
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Accepted', 'Exit Server Microfunc'])

    def test03AcceptNoTimeoutConnected(self):
        '''Test no-timeout connected in accept'''
        global resultsTest
        resultsTest = []
        clientAcceptWithTimeout()
        serverAcceptWithTimeout()
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Accepted', 'Exit Server Microfunc'])

    def test05RecvfromWithTimeoutExpired(self):
        '''Test expiring timeout in recvfrom'''
        global resultsTest
        resultsTest = []
        clientRecvfromWithTimeout()
        serverRecvfromWithTimeout(30)
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Timed Out', 'Exit Server Microfunc'])

    def test06RecvfromWithTimeout(self):
        '''Test timeout not expired in recvfrom'''
        global resultsTest
        resultsTest = []
        clientRecvfromWithTimeout()
        serverRecvfromWithTimeout(200)
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Received', 'Exit Server Microfunc'])

    def test07RecvfromNoTimeoutReceived(self):
        '''Test no-timeout received in recvfrom'''
        global resultsTest
        resultsTest = []
        clientRecvfromWithTimeout()
        serverRecvfromWithTimeout()
        xtime.swait(300)
        self.failUnlessEqual(resultsTest, ['Received', 'Exit Server Microfunc'])

    def test09ManyAccept(self):
        '''Test many accept with different timeouts'''
        global resultsTest
        resultsTest = []
        serverAcceptWithTimeout(200, 1)
        serverAcceptWithTimeout(300, 2)
        serverAcceptWithTimeout(600, 3)
        serverAcceptWithTimeout(400, 4)
        serverAcceptWithTimeout(230, 5)
        serverAcceptWithTimeout(100, 6)
        clientAcceptWithTimeout(500, 3)
        clientAcceptWithTimeout(150, 4)
        xtime.swait(800)
        self.failUnlessEqual(resultsTest, ['Timed Out 6', 'Exit Server Microfunc 6', 'Accepted 4', 'Exit Server Microfunc 4',
                       'Timed Out 1', 'Exit Server Microfunc 1', 'Timed Out 5', 'Exit Server Microfunc 5',
                       'Timed Out 2', 'Exit Server Microfunc 2',
                       'Accepted 3', 'Exit Server Microfunc 3'])

if __name__ == '__main__':
    unittest.main()

