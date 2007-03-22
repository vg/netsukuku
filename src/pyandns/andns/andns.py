from  _andns import *

_MAX_ID= 2**16 -1

#
##
# Bof, it's better to use the procedural api.
# Probably this class will be dropped.
##
#
class AndnsQuery(object):
    __slots__= [
            'id',
            'setId',
            'type',
            'setType',
            'realm',
            'setRealm',
            'proto',
            'setProto',
            'hashed',
            'setHashed',
            'service',
            'setService',
            'question',
            'setQuestion',
            'recursion',
            'setRecursion',
            'server',
            'setServer']

    def __init__(self, 
                id= None, 
                type= AT_A, 
                service= 0, 
                hashed= True, 
                question= None, 
                recursion= True, 
                proto= PROTO_TCP, 
                realm= NTK_REALM,
                server= "localhost"):

        self.id=            id
        self.type=          type
        self.realm=         realm
        self.proto=         proto
        self.hashed=        hashed
        self.server=        server
        self.service=       service
        self.question=      question
        self.recursion=     recursion

        sf= self.setField
        for ele in self.__slots__:
            if ele.startswith('set'): continue
            attr_name= "set%s%s" % (ele[0].upper(), ele[1:])
            setattr(self, attr_name, sf(ele))

    def setField(self, field):
        def fly_setField(value):
            setattr(self, field, value)
        return fly_setField

class AndnsAnswer(object):

    __slots__= [
            'wg',
            'prio',
            'rdata',
            'service',
            'main_ip']

    def __init__(self, main_ip, wg, prio, service, rdata):

        self.wg =       wg
        self.prio =     prio
        self.rdata=     rdata
        self.service =  service
        self.main_ip =  main_ip

    def __cmp__(self, other):
        res= self.prio- other.prio
        if res:
            return res
        return self.wg- other.wg

class AndnsPacket(object):

    __slots__= [
            'r',
            'z',
            'p',
            'id',
            'qr',
            'nk',
            'ipv',
            'qtype',
            'rcode',
            'ancount',
            'service',
            'qstdata',
            'pkt_answ']

    def __init__(self, id, r, qr, z, qtype, ancount,
                 ipv, nk, rcode, p, service, qstdata):

        self.r =        r
        self.z =        z
        self.p =        p
        self.id =       id
        self.qr =       qr
        self.nk =       nk
        self.ipv =      ipv
        self.qtype =    qtype
        self.rcode =    rcode
        self.ancount =  0
        self.service =  service
        self.qstdata =  qstdata

    def __len__(self):
        return self.ancount

    def addAnswer(self, answer):
        a= AndnsAnswer(*answer)
        self.pkt_answ.append(a)
        self.ancount+= 1

    def addAnswers(self, answers):
        for answ in answers:
            self.addAnswer(answ)

    def __iter__(self):
        for a in self.pkt_answ:
            yield a

    def orderAnswers(self):

        if self.qtype <> AT_A:
            raise "Non sense: Answers coulb be ordered if, and only if, qtype= AT_A"

        res= self.answers[:]
        res.sort()
        return res


def ntk_query(qst, id= None, recursion= True, hashed= True, 
        qtype= AT_A, realm= NTK_REALM, proto= PROTO_TCP, 
        service= 0, port= 53, server= '127.0.0.1'):

    if id is None:
        from random import randint
        id= randint(1, _MAX_ID)

    pkt= AndnsPacket(
            id= id, 
            p= proto, 
            nk= realm, 
            qtype= qtype, 
            r= recursion, 
            qstdata= qst,
            service= service) 

    atuples= _andns._py_ntk_query(id, recursion, hashed, 
            qtype, realm, proto, service, port, qst, server)

    pkt.addAnswers(atuples)

    return pkt

__all__= [
        "ntk_query",
        "AndnsAnswer",
        "AndnsPacket",
        "IPV4",
        "IPV6",
        "AT_A",
        "AT_G",
        "AT_PTR",
        "PROTO_TCP",
        "PROTO_UDP",
        "NTK_REALM",
        "INET_REALM",
        "RCODE_ERFSD",
        "RCODE_ENSDMN",
        "RCODE_ENIMPL",
        "RCODE_NOERROR",
        "RCODE_EINTRPRT",
        "RCODE_ESRVFAIL",
        "__andns_version__",
        ]
