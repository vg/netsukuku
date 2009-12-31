
import andns.andns as andns
import unittest

# typedef struct andns_pkt
# {
#     uint16_t        id;         /* id                   */
#     uint8_t         r;          /* recursion            */
#     uint8_t         qr;         /* question or answer?  */
#     uint8_t         z;          /* compression          */
#     uint8_t         qtype;      /* query type           */
#     uint16_t        ancount;    /* answers number       */
#     uint8_t         ipv;        /* ipv4 ipv6            */
#     uint8_t         nk;         /* ntk Bit              */
#     uint8_t         rcode;      /* response code        */
#     uint8_t         p;          /* snsd protocol        */
#     uint16_t        service;    /* snsd service         */
#     uint16_t        qstlength;  /* question lenght      */
#     char            *qstdata;   /* question             */
#     andns_pkt_data  *pkt_answ;  /* answres              */
# } andns_pkt;


class TestAndns(unittest.TestCase):

    def setUp(self):
        self.id = 1
        self.r = 1
        self.qr = 0 # only questions
        self.z = 0
        self.qtype = andns.AT_A # hostname to ip
        self.ancount = 0
        self.ipv = 0 # ipv4
        self.nk = 1
        self.rcode = 0
        self.p = 1
        self.service = 0 # Zero SNSD record
        self.qstdata = "asdd"

        self.packet = andns.AndnsPacket(self.id, self.r, self.qr, self.z,
                                        self.qtype, self.ancount, self.ipv,
                                        self.nk, self.rcode, self.p, self.service,
                                        self.qstdata, answer_tuples=[])

    def testConversion(self):
        self.failUnlessEqual(andns.from_wire(self.packet.to_wire()), self.packet);

if __name__ == "__main__":
    unittest.main()
