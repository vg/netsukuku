##
# This file is part of Netsukuku
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

##
# This is a fake DNS server. It receives requests made by classic DNS
# resolvers. It distinguishes the realm intended by the user (.ntk
# suffix is for netsukuku, the rest is for internet)
# Then it translates the DNS request in a ANDNS request, and forwards
# it to AndnsServer.
##

import dns.message
import dns.rrset
from random import randint

from ntk.wrap.sock import Sock
from ntk.network.inet import ip_to_str
from ntk.lib.micro import microfunc
from ntk.lib.log import logger as logging
from ntk.core.andna import NULL_SERV_KEY, make_serv_key
from ntk.core.snsd import AndnaResolvedRecord
from ntk.wrap.xtime import swait

# This class is a temporary hack
class AndnsRequest(object):
    def __init__(self):
        self.hostname = None
        self.ntk_bit = 0
        self.serv_key = NULL_SERV_KEY

# This class is a temporary hack
class AndnsServer(object):
    def __init__(self, andna):
        self.andna = andna
    def resolve(self, req):
        if req.ntk_bit:
            return self.andna.resolve(req.hostname, req.serv_key)
        else:
            raise Exception, 'Internet realm not implemented yet.'

class DnsWrapper(object):
    def __init__(self, maproute, andnsserver=None):
        self.maproute = maproute
        self.andnsserver = andnsserver
        # not None means use in-process method, None means use libandns.so
        self.run()

    @microfunc(True, keep_track=1)
    def run(self):
        socket = Sock(None, None)
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind(('', 53))
        logging.debug('DnsWrapper: binded to UDP port 53.')
        self.serving_ids = []

        while True:
            logging.debug('DnsWrapper: waiting requests.')
            message, address = self.s.recvfrom(1024)
            logging.debug('DnsWrapper: serving a request.')
            self.requestHandler(address, message)

    @microfunc(True, keep_track=1)
    def requestHandler(self, address, message):
        resp = None
        try:
            message_id = ord(message[0]) * 256 + ord(message[1])
            logging.debug('DnsWrapper: msg id = ' + str(message_id))
            if message_id in self.serving_ids:
                # the request is already taken, drop this message
                logging.debug('DnsWrapper: I am already serving this request.')
                return
            self.serving_ids.append(message_id)
            self.cleanup_serving_id(message_id)
            try:
                msg = dns.message.from_wire(message)
                try:
                    op = msg.opcode()
                    if op == 0:
                        # standard query
                        resp = self.std_qry(msg)
                    elif op == 1:
                        # inverse query
                        resp = self.inv_qry(msg)
                    else:
                        # not implemented
                        resp = self.make_response(qry=msg, RCODE=4)   # RCODE =  4    Not Implemented

                except Exception, e:
                    logging.info('DnsWrapper: got ' + repr(e))
                    resp = self.make_response(qry=msg, RCODE=2)   # RCODE =  3    Server Error
                    logging.debug('DnsWrapper: resp = ' + repr(resp.to_wire()))

            except Exception, e:
                logging.info('DnsWrapper: got ' + repr(e))
                resp = self.make_response(id=message_id, RCODE=1)   # RCODE =  3    Format Error
                logging.debug('DnsWrapper: resp = ' + repr(resp.to_wire()))

        except Exception, e:
            # message was crap, not even the ID
            logging.info('DnsWrapper: got ' + repr(e))

        if resp:
            self.s.sendto(resp.to_wire(), address)

    @microfunc(True)
    def cleanup_serving_id(self, message_id):
        swait(20000)
        self.serving_ids.remove(message_id)

    def std_qry(self, msg):
        ''' Handles a standard DNS query. Returns the Message to use as response.

        @param msg: A standard DNS query
        @type msg: dns.message.Message object
        @rtype: dns.message.Message object'''

        qs = msg.question
        logging.debug('DnsWrapper: ' + str(len(qs)) + ' questions.')

        answers = []
        nxdomain = False
        for q in qs:
            qname = q.name.to_text()[:-1]
            logging.debug('DnsWrapper: q name = ' + qname)
            ipstr = None
            if qname:
                # Netsukuku or Internet?
                realm_internet = True
                if qname[-4:].upper() == '.NTK':
                    qname = qname[:-4]
                    realm_internet = False
                # Transform DNS request in ANDNS request
                andns_req = AndnsRequest()
                andns_req.hostname = qname
                andns_req.ntk_bit = 0 if realm_internet else 1
                # TODO interpret requests of type SRV records (andns_req.serv_key)
                logging.debug('DnsWrapper: andns_req = ' + str(andns_req))

                ret = None
                if self.andnsserver:
                    logging.debug('DnsWrapper: andnsserver.resolve...')
                    ret = self.andnsserver.resolve(andns_req)
                    logging.debug('DnsWrapper: andnsserver.resolve returns ' + str(ret))
                    if ret[0] == 'OK':
                        ret = ret[1]
                    else:
                        logging.debug('DnsWrapper: returning error because ret: ' + str(ret))
                        ret = None
                        nxdomain = True
                        break
                else:
                    # TODO: call libandns
                    logging.debug('DnsWrapper: call libandns not implemented yet.')
                    nxdomain = True
                    break

                if ret is not None:
                    if isinstance(ret, AndnaResolvedRecord):
                        # ret is an instance of AndnaResolvedRecord
                        if ret.records is None:
                            logging.debug('DnsWrapper: not found.')
                            break
                        else:
                            # Since in DNS, records A have no priority or weight, we do like this:
                            # choose max priority records
                            max_prio = -1
                            for rec in ret.records:
                                if max_prio == -1 or max_prio > rec.priority:
                                    max_prio = rec.priority
                            records = [rec for rec in ret.records if rec.priority == max_prio]
                            # return them all
                            for rec in records:
                                # from a nip to a ip string
                                ipstr = ip_to_str(self.maproute.nip_to_ip(rec.record))

                                logging.debug('DnsWrapper: returns: ' + ipstr)
                                rrset = dns.rrset.from_text(q.name, ret.get_ttl() / 1000,
                                       dns.rdataclass.IN, dns.rdatatype.A, ipstr)
                                answers.append(rrset)
                    else:
                        # ret is an instance of AndnsAnswer ??
                        # TODO
                        pass

        if not nxdomain:
            resp = self.make_response(qry=msg)
            for rrset in answers:
                resp.answer.append(rrset)
            logging.debug('DnsWrapper: resp = ' + repr(resp.to_wire()))
        else:
            # TODO what has to do a dns server if some of the questions is not found?
            logging.info('DnsWrapper: some question not answered.')
            resp = self.make_response(qry=msg, RCODE=3)   # RCODE =  3    Name Error
            logging.debug('DnsWrapper: resp = ' + repr(resp.to_wire()))

        return resp

    def inv_qry(self, msg):
        ''' Handles a inverse DNS query. Returns the Message to use as response.

        @param msg: A inverse DNS query
        @type msg: dns.message.Message object
        @rtype: dns.message.Message object'''

        # TODO
        resp = self.make_response(qry=msg, RCODE=4)   # RCODE =  4    Not Implemented
        return resp

    def make_response(self, qry=None, id=None, AA=True, RA=True, RCODE=0):
        ''' Makes a response, from a query or just an ID.

        @rtype: dns.message.Message object'''

        if qry is None and id is None:
            raise Exception, 'bad use of make_response'
        if qry is None:
            resp = dns.message.Message(id)
            # QR = 1
            resp.flags |= dns.flags.QR
            if RCODE == 1:
                raise Exception, 'bad use of make_response'
        else:
            resp = dns.message.make_response(qry)
        if AA:
            resp.flags |= dns.flags.AA
        else:
            resp.flags &= 0xffff - dns.flags.AA
        if RA:
            resp.flags |= dns.flags.RA
        else:
            resp.flags &= 0xffff - dns.flags.RA
        resp.set_rcode(RCODE)
        return resp

