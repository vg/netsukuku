/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

#include "inet.h"
#include "pkts.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "netsukuku.h"
#include "request.h"
#include "accept.h"
#include "xmalloc.h"
#include "log.h"

/*Handy functions to build the PACKET*/
void pkt_addfrom(PACKET *pkt, inet_prefix *from)
{
	memcpy(&pkt->from, from, sizeof(inet_prefix));
}

void pkt_addto(PACKET *pkt, inet_prefix *to)
{
	memcpy(&pkt->to, to, sizeof(inet_prefix));
}

void pkt_addsk(PACKET *pkt, int family, int sk, int sk_type)
{
	pkt->family=family;
	pkt->sk=sk;
	pkt->sk_type=sk_type;
}

void pkt_addport(PACKET *pkt, u_short port)
{
	pkt->port=port;
}

void pkt_addflags(PACKET *pkt, int flags)
{
	if(flags)
		pkt->flags=flags;
}

void pkt_addhdr(PACKET *pkt, pkt_hdr *hdr)
{
	memcpy(&pkt->hdr, hdr, sizeof(pkt_hdr));
}

void pkt_addmsg(PACKET *pkt, char *msg)
{
	pkt->msg=msg;
}
/*End of handy stupid functions (^_+)*/

void pkt_free(PACKET *pkt, int close_socket)
{
	if(pkt->sk && close_socket) {
		close(pkt->sk);
		pkt->sk=0;
	}
	
	if(pkt->msg) {
		xfree(pkt->msg);
		pkt->msg=0;
	}
}

char *pkt_pack(PACKET *pkt)
{
	char *buf;
	
	buf=(char *)xmalloc(PACKET_SZ(pkt->hdr.sz));
	memcpy(buf, &pkt->hdr, sizeof(pkt_hdr));
	
	if(pkt->hdr.sz)
		memcpy(buf+sizeof(pkt_hdr), pkt->msg, pkt->hdr.sz);
	
	return buf;
}

PACKET *pkt_unpack(char *pkt)
{
	PACKET *pk;

	pk=(PACKET *)xmalloc(sizeof(PACKET));
	
	/*Now, we extract the pkt_hdr...*/
	memcpy(&pk->hdr, pkt, sizeof(pkt_hdr));
	/*and verify it...*/
	if(pkt_verify_hdr(*pk)) {
		debug(DBG_NOISE, "Error while unpacking the PACKET. "
				"Malformed header");
		return 0;
	}
	
	pk->msg=pkt+sizeof(pkt_hdr);
	return pk;
}
	
int pkt_verify_hdr(PACKET pkt)
{
	if(strncmp(pkt.hdr.ntk_id, NETSUKUKU_ID, 3))
		return 1;
	if(pkt.hdr.sz > MAXMSGSZ)
		return 1;
	
	return 0;
}

ssize_t pkt_send(PACKET *pkt)
{
	ssize_t ret;
	char *buf=0;

	buf=pkt_pack(pkt);

	if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST) {
		struct sockaddr to;
		socklen_t tolen;
		
		if(inet_to_sockaddr(&pkt->to, pkt->port, &to, &tolen) < 0) {
			debug(DBG_NOISE, "Cannot pkt_send(): %d "
					"Family not supported", pkt->to.family);
			ret=-1;
			goto finish;
		}
		
		ret=inet_sendto(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz), 
				pkt->flags, &to, sizeof(to));
	} else if(pkt->sk_type==SKT_TCP)
		ret=inet_send(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz), pkt->flags);
	else
		fatal("Unkown socket_type. Something's very wrong!! Be aware");

finish:
	if(buf)
		xfree(buf);
	return ret;
}

ssize_t pkt_recv(PACKET *pkt)
{
	ssize_t err=-1;
	struct sockaddr from;
	socklen_t fromlen;

	if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST) {	
		char *buf[MAXMSGSZ];
		
		memset(buf, 0, MAXMSGSZ);
		memset(&from, 0, sizeof(struct sockaddr));
		
		if(pkt->family == AF_INET)
			fromlen=sizeof(struct sockaddr_in);
		else if(pkt->family == AF_INET6)
			fromlen=sizeof(struct sockaddr_in6);
		else {
			error("pkt_recv udp: family not set");
			return -1;
		}

		/* we get the whole pkt, */
		err=inet_recvfrom(pkt->sk, buf, sizeof(pkt_hdr)+MAXMSGSZ, 
				pkt->flags, &from, &fromlen);
		if(err < sizeof(pkt_hdr)) {
			debug(DBG_NOISE, "inet_recvfrom() of the hdr aborted!");
			return -1;
		}
		
		/* then we extract... and verify it */
		memcpy(&pkt->hdr, buf, sizeof(pkt_hdr));
		if(pkt_verify_hdr(*pkt) || pkt->hdr.sz+sizeof(pkt_hdr) > err) {
			debug(DBG_NOISE, "Error while unpacking the PACKET."
					" Malformed header");
			return -1;
		}

		if(sockaddr_to_inet(&from, &pkt->from, &pkt->port) < 0) {
			debug(DBG_NOISE, "Cannot pkt_recv(): %d"
					" Family not supported", from.sa_family);
			return -1;
		}
		
		pkt->msg=0;
		if(pkt->hdr.sz) {
			/*let's get the body*/
			pkt->msg=xmalloc(pkt->hdr.sz);
			memcpy(pkt->msg, buf+sizeof(pkt_hdr), pkt->hdr.sz);
		}
	} else if(pkt->sk_type==SKT_TCP) {
		/*we get the hdr...*/
		err=inet_recv(pkt->sk, &pkt->hdr, sizeof(pkt_hdr), pkt->flags);
		if(err != sizeof(pkt_hdr)) {
			debug(DBG_NOISE, "inet_recv() of the hdr aborted!");
			return -1;
		}
		/*...and verify it*/
		if(pkt_verify_hdr(*pkt)) {
			debug(DBG_NOISE, "Error while unpacking the PACKET. Malformed header");
			return -1;
		}

		pkt->msg=0;
		if(pkt->hdr.sz) {
			/*let's get the body*/
			pkt->msg=xmalloc(pkt->hdr.sz);
			err=inet_recv(pkt->sk, pkt->msg, pkt->hdr.sz, pkt->flags);
			if(err != pkt->hdr.sz) {
				debug(DBG_NOISE, "Cannot inet_recv() the pkt's body");
				return -1;
			}
		}
	} else
		fatal("Unkown socket_type. Something's very wrong!! Be aware");
	
	return err;
}

int pkt_tcp_connect(inet_prefix *host, short port)
{
	int sk;
	PACKET pkt;
	char *ntop;
	ssize_t err;

	ntop=inet_to_str(*host);
	
	if((sk=new_tcp_conn(host, port))==-1)
		goto finish;
	
	/*
	 * Now we receive the first pkt from the srv. It is an ack. 
	 * Let's hope it isn't NEGATIVE (-_+)
	 */
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addsk(&pkt, host->family, sk, SKT_TCP);
	pkt_addflags(&pkt, MSG_WAITALL);

	if((err=pkt_recv(&pkt)) < 0) {
		error("Connection to %s failed: it wasn't possible to receive "
				"the ACK", ntop);
		sk=-1;
		goto finish;
	}
	
	/* ...Last famous words */
	if(pkt.hdr.op==ACK_NEGATIVE) {
		u_char err;
		
		memcpy(&err, pkt.msg, pkt.hdr.sz);
		error("Cannot connect to %s: %s", ntop, rq_strerror(err));
		sk=-1;
		goto finish;
	}
	
finish:
	if(ntop)
		xfree(ntop);
	pkt_free(&pkt, 0);
	return sk;
}

void pkt_fill_hdr(pkt_hdr *hdr, u_char flags, int id, u_char op, size_t sz)
{
	hdr->ntk_id[0]='n';
	hdr->ntk_id[1]='t';
	hdr->ntk_id[2]='k';

	if(!id)
		id=random();
		
	hdr->id=id;
	hdr->flags=flags;
	hdr->op=op;
	hdr->sz=sz;
}

/* send_rq: This functions send a `rq' request, with an id set to `rq_id', to `pkt->to'. 
 * If `pkt->hdr.sz` is > 0 it includes the `pkt->msg' in the packet otherwise it will be NULL. 
 * If `rpkt' is not null it will receive and store the reply pkt in `rpkt'.
 * If `check_ack' is set send_rq, send_rq attempts to receive the reply pkt and it checks its
 * ACK and its id; if the test fails it gives an appropriate error message.
 * If `re'  is not null send_rq confronts the OP of the received reply pkt 
 * with `re'; if the test fails it gives an appropriate error message.
 * If an error occurr send_rq returns -1 otherwise it returns 0.
 */
int send_rq(PACKET *pkt, int flags, u_char rq, int rq_id, u_char re, int check_ack, PACKET *rpkt)
{
	u_short port;
	ssize_t err;
	int ret=0;
	char *ntop=0, *rq_str, *re_str;
	u_char hdr_flags=0;
	

	if(op_verify(rq)) {
		error("\"%s\" request/reply is not valid!", rq_str);
		return -1;
	}
	if(!re_verify(rq))
		rq_str=re_to_str(rq);
	else
		rq_str=rq_to_str(rq);

	if(re && re_verify(re)) {
		error("\"%s\" reply is not valid!", re_str);
		return -1;
	}

	if(rpkt)
		memset(rpkt, '\0', sizeof(PACKET));

	ntop=inet_to_str(pkt->to);
	debug(DBG_INSANE, "Sending the %s op to %s", rq_str, ntop);

	/* * * the request building process * * */
	if(check_ack)
		hdr_flags|=SEND_ACK;
	if(me.cur_node->flags & MAP_HNODE)
		hdr_flags|=HOOK_PKT;
	pkt_fill_hdr(&pkt->hdr, hdr_flags, rq_id, rq, pkt->hdr.sz);
	if(!pkt->hdr.sz)
		pkt->msg=0;
	
	if(pkt->sk_type==SKT_TCP) {
		pkt_addport(pkt, ntk_tcp_port);
	} else if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST)
		pkt_addport(pkt, ntk_udp_port);
	if(rpkt)
		pkt_addport(rpkt, pkt->port);

	pkt_addflags(pkt, flags);

	if(!pkt->sk) {
		if(!pkt->to.family || !pkt->to.len) {
			error("pkt->to isn't set. I can't create the new connection");
			ret=-1;
			goto finish;
		}
		
		if(pkt->sk_type==SKT_TCP)
			pkt->sk=pkt_tcp_connect(&pkt->to, pkt->port);
		else if(pkt->sk_type==SKT_UDP)
			pkt->sk=new_udp_conn(&pkt->to, pkt->port);
		else if(pkt->sk_type==SKT_BCAST)
			pkt->sk=new_bcast_conn(&pkt->to, pkt->port, me.cur_dev_idx);
		else
			fatal("Unkown socket_type. Something's very wrong!! Be aware");

		if(pkt->sk==-1) {
			error("Couldn't connect to %s to launch the %s request", ntop, rq_str);
			ret=-1; 
			goto finish;
		}
	}
	if(rpkt) {
		pkt_addsk(rpkt, pkt->to.family, pkt->sk, pkt->sk_type);
		pkt_addflags(rpkt, MSG_WAITALL);
	}
	
	/*Let's send the request*/
	err=pkt_send(pkt);
	if(err==-1) {
		error("Cannot send the %s request to %s.", rq_str, ntop);
		ret=-1; 
		goto finish;
	}

	/* * * the reply * * */
	if(rpkt) {
		debug(DBG_NOISE, "Trying to receive the reply for the %s request with %d id", rq_str, pkt->hdr.id);
		if(pkt->sk_type==SKT_UDP)
			memcpy(&rpkt->from, &pkt->to, sizeof(inet_prefix));

		if((err=pkt_recv(rpkt))==-1) {
			error("Error while receving the reply for the %s request from %s.", rq_str, ntop);
			ret=-1; 
			goto finish;
		}

		if(rpkt->hdr.op == ACK_NEGATIVE && check_ack) {
			u_char err_ack;
			memcpy(&err_ack, rpkt->msg, sizeof(u_char));
			error("%s failed. The node %s replied: %s", rq_str, ntop, rq_strerror(err_ack));
			ret=-1; 
			goto finish;
		} else if(re && rpkt->hdr.op != re && check_ack) {
			error("The node %s replied %s but we asked %s!", ntop, re_to_str(rpkt->hdr.op), re_str);
			ret=-1;
			goto finish;
		}

		if(check_ack && rpkt->hdr.id != pkt->hdr.id) {
			error("The id (%d) of the reply (%s) doesn't match the id of our request (%d)", rpkt->hdr.id, 
					re_str, pkt->hdr.id);
			ret=-1;
			goto finish;
		}
	}

finish:
	if(ntop)
		xfree(ntop);
	return ret;
}

/* 
 * pkt_err: Sends back to "pkt.from" an error pkt, with ACK_NEGATIVE, 
 * containing the "err" code.
 */
int pkt_err(PACKET pkt, u_char err)
{
	char *msg;
	
	memcpy(&pkt.to, &pkt.from, sizeof(inet_prefix));
	pkt_fill_hdr(&pkt.hdr, 0, pkt.hdr.id, ACK_NEGATIVE, sizeof(int));
	
	pkt.msg=msg=xmalloc(sizeof(u_char));
	memcpy(msg, &err, sizeof(u_char));
		
	err=pkt_send(&pkt);
	pkt_free(&pkt, 0);
	return err;
}

	
int pkt_exec(PACKET pkt, int acpt_idx)
{
	char *ntop, *op_str;
	int err=0;

	if(!re_verify(pkt.hdr.op))
		op_str=re_to_str(pkt.hdr.op);
	else
		op_str=rq_to_str(pkt.hdr.op);

	debug(DBG_INSANE, "pkt_exec(): id: %d, op: %s, acpt_idx: %d", pkt.hdr.id,
			op_str, acpt_idx);
	if((err=add_rq(pkt.hdr.op, &accept_tbl[acpt_idx].rqtbl))) {
		ntop=inet_to_str(pkt.from);
		error("From %s: Cannot process the %s request: %s", ntop, 
				op_str, rq_strerror(err));
		if(ntop)
			xfree(ntop);
		pkt_err(pkt, err);
		return -1;
	}

	switch(pkt.hdr.op) 
	{
		case ECHO_ME:
			err=radard(pkt);
			break;
		case ECHO_REPLY:
			err=radar_recv_reply(pkt);
			break;
			
		case GET_FREE_NODES:
			err=put_free_nodes(pkt);
			break;
		case GET_INT_MAP:
			err=put_int_map(pkt);
			break;
		case GET_BNODE_MAP:
			err=put_bnode_map(pkt);
			break;
		case GET_EXT_MAP:
			err=put_ext_map(pkt);
			break;

		case TRACER_PKT:
		case TRACER_PKT_CONNECT:
			tracer_pkt_recv(pkt);
			break;
		case QSPN_CLOSE:
			err=qspn_close(pkt);
			break;
		case QSPN_OPEN:
			err=qspn_open(pkt);
			break;

		default:
			/* never reached */
			break;
	}
	
	return err;
}
