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
        self.rdata =    rdata
        self.service =  service
        self.main_ip =  main_ip

    def __cmp__(self, other):
        res= self.prio- other.prio
        if res:
            return res
        return self.wg- other.wg

    def to_tuple(self):
        # if it is a NIP then convert to string
        if (isinstance(self.rdata, list)):
            self.rdata = ".".join([str(n) for n in reversed(self.rdata)])
        return (self.main_ip, self.wg, self.prio, self.service, self.rdata)
        
    def __repr__(self):
        return "\n-------------------------------\n"+\
               "  weight="+str(self.wg)+        "\n"+\
               "  priority="+str(self.prio)+    "\n"+\
               "  rdata="+str(self.rdata)+      "\n"+\
               "  service="+str(self.service)+  "\n"+\
               "  main ip="+str(self.main_ip)+  "\n"+\
               "---------------------------------\n"
                       
def from_wire(wired):
    """ Return AndnsPacket instances from data taken by socket"""
    return AndnsPacket(*pkt_to_tuple(wired))
         
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

    def __init__(self, id, r=1, qr=0, z=0, qtype=0, ancount=0,
                 ipv=0, nk=1, rcode=0, p=1, service=0, qstdata='', answer_tuples=[]):

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
        self.pkt_answ = []
        for answer in answer_tuples:
            self.addAnswer(answer)
            
    def __len__(self):
        return self.ancount
    
    def __repr__(self):
        from binascii import hexlify
        return "id="+str(self.id)+                  "\n"+\
               "r="+str(self.r)+                    "\n"+\
               "z="+str(self.z)+                    "\n"+\
               "p="+str(self.p)+                    "\n"+\
               "qr="+str(self.qr)+                  "\n"+\
               "nk="+str(self.nk)+                  "\n"+\
               "i="+str(self.ipv)+                  "\n"+\
               "qtype="+str(self.qtype)+            "\n"+\
               "rcode="+str(self.rcode)+            "\n"+\
               "ancount="+str(self.ancount)+        "\n"+\
               "service="+str(self.service)+        "\n"+\
               "qstdata="+str(self.qstdata)+        "\n"+\
               "answers..."+"\n"+str(self.pkt_answ)+"\n" 
               
    def addAnswer(self, answer):
        a= AndnsAnswer(*answer)
        self.pkt_answ.append(a)
        self.ancount+= 1

    def addAnswers(self, answers):
        """ Accept a sequence of AndnsAnswer instances """
        for answ in answers:
            self.addAnswer(answ)
            
    def to_wire(self):
        data_tuple = (self.id, self.r, self.qr, self.z, self.qtype, self.ancount,
               self.ipv, self.nk, self.rcode, self.p, self.service, self.qstdata,
                 [ answ.to_tuple() for answ in self.pkt_answ ])
        return tuple_to_pkt(*data_tuple)
    
    def __iter__(self):
        for a in self.pkt_answ:
            yield a

    def orderAnswers(self):

        if self.qtype <> AT_A:
            raise "Non sense: Answers coulb be ordered if, and only if, qtype= AT_A"

        res= self.answers[:]
        res.sort()
        return res


def query(qst, id= None, recursion= 1, hashed= True, 
        qtype= AT_A, realm= NTK_REALM, proto= PROTO_UDP, 
        service= 0, port= 53, server= '127.0.0.1'):

    if id is None:
        from random import randint
        id= randint(1, _MAX_ID)

    atuples= ntk_query(id, recursion, hashed, 
            qtype, realm, proto, service, port, qst, server)

    pkt= AndnsPacket(
            id= id, 
            p= proto, 
            qr= 1, 
            z= 1, 
            ancount= len(atuples),
            ipv= 0,
            rcode= 0,
            nk= realm, 
            qtype= qtype, 
            r= recursion, 
            qstdata= qst,
            service= service) 
            
    pkt.addAnswers(atuples)

    return pkt

__all__= [
        "query",
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
