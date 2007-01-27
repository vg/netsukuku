/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
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
 *
 * --
 * pkts.c
 *
 * General functions to forge, pack, send, receive, forward and unpack
 * packets. See pkts.h for the API description.
 */

#include "includes.h"
#include <zlib.h>

#include "inet.h"
#include "request.h"
#include "endianness.h"
#include "pkts.h"
#include "common.h"

struct pkt_op_table *pkt_op_tbl=0;
int    pkt_op_tbl_counter=0;
u_char pkt_op_tbl_sorted=0;

interface cur_ifs[MAX_INTERFACES];
int cur_ifs_n;

/*
 * pkts_init
 *
 *
 * Initialize the vital organs of the pkts.c's functions.
 *
 * `ifs' is the array which keeps all the the `ifs_n'# network 
 * interface that will be used.
 *
 * If `queue_init' is not 0, the pkt_queue is initialized too.
 */
void pkts_init(interface *ifs, int ifs_n, int queue_init)
{
	/* register the ACK replies */
	RQ_ADD_REQUEST(ACK_AFFERMATIVE, RQ_REPLY);
	RQ_ADD_REQUEST(ACK_NEGATIVE,    RQ_REPLY);

	/* request errors */
	RQERR_ADD_ERROR(E_INVALID_PKT, "Invalid packet");

	cur_ifs_n = ifs_n > MAX_INTERFACES ? ifs_n : MAX_INTERFACES;
	memcpy(cur_ifs, ifs, sizeof(interface)*cur_ifs_n);
	
	pkt_q_counter=0;
	if(queue_init)
		pkt_queue_init();
	
	op_filter_reset(OP_FILTER_ALLOW);
}


/*\
 *
 * 	* * *  Handy functions to build the PACKET  * * * 
 *
\*/

void pkt_addfrom(PACKET *pkt, inet_prefix *from)
{
	if(!from)
		setzero(&pkt->from, sizeof(inet_prefix));
	else
		inet_copy(&pkt->from, from);
}

void pkt_addto(PACKET *pkt, inet_prefix *to)
{
	if(!to)
		setzero(&pkt->to, sizeof(inet_prefix));
	else
		inet_copy(&pkt->to, to);
}

void pkt_add_dev(PACKET *pkt, interface *dev, int bind_the_socket)
{
	pkt->dev=dev;
	if(dev && bind_the_socket)
		pkt->pkt_flags|=PKT_BIND_DEV;
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

void pkt_addtimeout(PACKET *pkt, u_int timeout, int recv, int send)
{
	if((pkt->timeout=timeout)) {
		if(recv)
			pkt->pkt_flags|=PKT_RECV_TIMEOUT;
		if(send)
			pkt->pkt_flags|=PKT_SEND_TIMEOUT;
	}
}

void pkt_addcompress(PACKET *pkt)
{
	pkt->pkt_flags|=PKT_COMPRESSED;
}

void pkt_addlowdelay(PACKET *pkt)
{
	pkt->pkt_flags|=PKT_SET_LOWDELAY;
}

void pkt_addnonblock(PACKET *pkt)
{
	pkt->pkt_flags|=PKT_NONBLOCK;
}

void pkt_addhdr(PACKET *pkt, pkt_hdr *hdr)
{
	if(!hdr)
		setzero(&pkt->hdr, sizeof(pkt_hdr));
	else
		memcpy(&pkt->hdr, hdr, sizeof(pkt_hdr));
}

void pkt_addmsg(PACKET *pkt, char *msg)
{
	pkt->msg=msg;
}
/* * * End of handy stupid functions (^_+) * * */


/* 
 * pkt_clear: blanks the entire PACKET struct, leaving intact only `hdr' 
 * and `msg' 
 */
void pkt_clear(PACKET *pkt)
{
	pkt_addfrom(pkt, 0);
	pkt_addto(pkt, 0);
	pkt_addsk(pkt, 0,0,0);
	pkt_addport(pkt, 0);
	pkt->flags=pkt->pkt_flags=0;
}

/* 
 * pkt_copy: Copy the `src' PACKET in `dst'. It xmallocs also a new msg block in
 * `dst->msg' of `src->hdr.sz' size and copies in it `src->msg'
 */
void pkt_copy(PACKET *dst, PACKET *src)
{
	memcpy(dst, src, sizeof(PACKET));
	
	if(src->hdr.sz && src->msg) {
		dst->msg=xmalloc(src->hdr.sz);
		memcpy(dst->msg, src->msg, src->hdr.sz);
	}
}


void pkt_free(PACKET *pkt, int close_socket)
{
	if(close_socket && pkt->sk)
		inet_close(&pkt->sk);
	
	if(pkt->msg) {
		xfree(pkt->msg);
		pkt->msg=0;
	}
}

/*
 * pkt_compress
 *
 * It compresses `pkt'->msg and stores the result in `dst'.
 * `dst_msg' must have at least `newhdr'->sz bytes big.
 * It is also assumed that `pkt'->msg is not 0.
 *
 * The size of the compressed msg is stored in `newhdr'->sz, while
 * the size of the orignal one is written in `newhdr'->uncompress_sz.
 * If the compression doesn't fail, `newhdr'->sz will be always less than
 * `newhdr'->uncompress_sz.
 *
 * Nothing in `pkt' is modified.
 *
 * If the packet was compressed  0 is returned and the COMPRESSED_PKT flag is
 * set to `newhdr'->.flags.
 * On error a negative value is returned.
 */
int pkt_compress(PACKET *pkt, pkt_hdr *newhdr, char *dst_msg)
{
	uLongf bound_sz;
	int ret;

	bound_sz=compressBound(pkt->hdr.sz);

	unsigned char dst[bound_sz];

	ret=compress2(dst, &bound_sz, (u_char*)pkt->msg, pkt->hdr.sz, 
			PKT_COMPRESS_LEVEL);
	if(ret != Z_OK) {
		error(RED(ERROR_MSG) "cannot compress the pkt. "
				"It will be sent uncompressed.", ERROR_FUNC);
		return -1;
	}

	if(bound_sz >= pkt->hdr.sz)
		/* Disgregard compression, it isn't useful in this case */
		return -pkt->hdr.sz;

	memcpy(dst_msg, dst, bound_sz);
	newhdr->uncompress_sz=pkt->hdr.sz;
	newhdr->sz=bound_sz;
	newhdr->flags|=COMPRESSED_PKT;

	return 0;
}

/*
 * pkt_pack
 *
 * It packs the packet with its `pkt'->header in a single buffer.
 * If PKT_COMPRESSED is set in `pkt'->pkt_flags, `pkt'->msg will be compressed
 * if its size is > PKT_COMPRESS_THRESHOLD.
 */
char *pkt_pack(PACKET *pkt)
{
	char *buf, *buf_hdr, *buf_body;
	
	buf=(char *)xmalloc(PACKET_SZ(pkt->hdr.sz));
	buf_hdr=buf;
	buf_body=buf+sizeof(pkt_hdr);

	/***
	 * Copy the header
	 */
	memcpy(buf_hdr, &pkt->hdr, sizeof(pkt_hdr));

	/* host -> network order */
	ints_host_to_network(buf_hdr, pkt_hdr_iinfo);
	/***/
	
	if(pkt->hdr.sz) {

                /*
		 * compress the packet if necessary 
		 */
                if((pkt->pkt_flags & PKT_COMPRESSED && 
				pkt->hdr.sz >= PKT_COMPRESS_THRESHOLD)) {
			
			if(!pkt_compress(pkt, &pkt->hdr, buf_body)) {
				/* 
				 * Re-copy the header in `buf', because
				 * it has been changed during compression. */
				memcpy(buf_hdr, &pkt->hdr, sizeof(pkt_hdr));
				ints_host_to_network(buf_hdr, pkt_hdr_iinfo);
			}
		} else
			/* Just copy the body of the packet */
			memcpy(buf_body, pkt->msg, pkt->hdr.sz);
		/**/
	}
	
	return buf;
}

/*
 * pkt_uncompress
 *
 * It uncompress the compressed `pkt' and stores the result in `pkt' itself
 * On error -1 is returned.
 */
int pkt_uncompress(PACKET *pkt)
{
	uLongf dstlen;
	int ret=0;
	unsigned char *dst=0;
	
	dstlen=pkt->hdr.uncompress_sz;
	dst=xmalloc(dstlen);
	
	ret=uncompress(dst, &dstlen, (u_char*) pkt->msg, pkt->hdr.sz);
	if(ret != Z_OK)
		ERROR_FINISH(ret, -1, finish);
	else
		ret=0;

	/**
	 * Restore the uncompressed packet
	 */
	xfree(pkt->msg);
	pkt->msg=(char*)dst;
	pkt->hdr.sz=pkt->hdr.uncompress_sz;
	pkt->hdr.uncompress_sz=0;
	pkt->hdr.flags&=~COMPRESSED_PKT;
	/**/

finish:
	if(ret && dst)
		xfree(dst);
	return ret;
}

/*
 * pkt_unpack
 *
 * `pkt' must be already in host order
 */
int pkt_unpack(PACKET *pkt)
{
	if(pkt->hdr.sz && pkt->msg && 
			pkt->hdr.flags & COMPRESSED_PKT)
		if(pkt_uncompress(pkt))
			return -1;

	return 0;
}

int pkt_verify_hdr(PACKET pkt)
{
	if(strncmp(pkt.hdr.ntk_id, NETSUKUKU_ID, 3) ||
			pkt.hdr.sz > MAXMSGSZ)
		return 1;

	if(pkt.hdr.flags & COMPRESSED_PKT && 
			(pkt.hdr.sz >= pkt.hdr.uncompress_sz ||
			 pkt.hdr.uncompress_sz > PKT_MAX_MSG_SZ))
		/* Invalid compression */
		return 1;

	return 0;
}

ssize_t pkt_send(PACKET *pkt)
{
	ssize_t ret=0;
	char *buf=0;

	buf=pkt_pack(pkt);

	if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST) {
		struct sockaddr_storage saddr_sto;
		struct sockaddr *to = (struct sockaddr *)&saddr_sto;
		socklen_t tolen;
		
		if(inet_to_sockaddr(&pkt->to, pkt->port, to, &tolen) < 0) {
			debug(DBG_NOISE, "Cannot pkt_send(): %d "
					"Family not supported", pkt->to.family);
			ERROR_FINISH(ret, -1, finish);
		}
		
		if(pkt->pkt_flags & PKT_SEND_TIMEOUT)
			ret=inet_sendto_timeout(pkt->sk, buf, 
					PACKET_SZ(pkt->hdr.sz), pkt->flags, to,
					tolen, pkt->timeout);
		else
			ret=inet_sendto(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz),
				pkt->flags, to, tolen);

	} else if(pkt->sk_type==SKT_TCP) {
		if(pkt->pkt_flags & PKT_SEND_TIMEOUT)
			ret=inet_send_timeout(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz),
					pkt->flags, pkt->timeout);
		else
			ret=inet_send(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz), 
					pkt->flags);
	} else
		fatal("Unkown socket_type. Something's very wrong!! Be aware");

finish:
	if(buf)
		xfree(buf);
	return ret;
}

ssize_t pkt_recv_udp(PACKET *pkt)
{
	ssize_t err=-1;
	struct sockaddr from;
	socklen_t fromlen;
	char buf[MAXMSGSZ];

	setzero(buf, MAXMSGSZ);
	setzero(&from, sizeof(struct sockaddr));

	if(pkt->family == AF_INET)
		fromlen=sizeof(struct sockaddr_in);
	else if(pkt->family == AF_INET6)
		fromlen=sizeof(struct sockaddr_in6);
	else {
		error("pkt_recv udp: family not set");
		return -1;
	}

	/* we get the whole pkt, */
	if(pkt->pkt_flags & PKT_RECV_TIMEOUT)
		err=inet_recvfrom_timeout(pkt->sk, buf, PACKET_SZ(MAXMSGSZ),
				pkt->flags, &from, &fromlen, pkt->timeout);
	else
		err=inet_recvfrom(pkt->sk, buf, PACKET_SZ(MAXMSGSZ), 
				pkt->flags, &from, &fromlen);

	if(err < sizeof(pkt_hdr)) {
		debug(DBG_NOISE, "inet_recvfrom() of the hdr aborted!");
		return -1;
	}

	/* then we extract the hdr... and verify it */
	memcpy(&pkt->hdr, buf, sizeof(pkt_hdr));
	/* network -> host order */
	ints_network_to_host(&pkt->hdr, pkt_hdr_iinfo);
	if(pkt_verify_hdr(*pkt) || pkt->hdr.sz+sizeof(pkt_hdr) > err) {
		debug(DBG_NOISE, RED(ERROR_MSG) "Malformed header", ERROR_POS);
		return -1;
	}

	if(sockaddr_to_inet(&from, &pkt->from, 0) < 0) {
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
	
	return err;
}

ssize_t pkt_recv_tcp(PACKET *pkt)
{
	ssize_t err=-1;

	/* we get the hdr... */
	if(pkt->pkt_flags & PKT_RECV_TIMEOUT)
		err=inet_recv_timeout(pkt->sk, &pkt->hdr, sizeof(pkt_hdr),
				pkt->flags, pkt->timeout);
	else
		err=inet_recv(pkt->sk, &pkt->hdr, sizeof(pkt_hdr), 
				pkt->flags);
	if(err != sizeof(pkt_hdr))
		return -1;

	/* ...and verify it */
	ints_network_to_host(&pkt->hdr, pkt_hdr_iinfo);
	if(pkt_verify_hdr(*pkt)) {
		debug(DBG_NOISE, RED(ERROR_MSG) "Malformed header", ERROR_POS);
		return -1;
	}

	pkt->msg=0;
	if(pkt->hdr.sz) {
		/* let's get the body */
		pkt->msg=xmalloc(pkt->hdr.sz);

		if(pkt->pkt_flags & PKT_RECV_TIMEOUT)
			err=inet_recv_timeout(pkt->sk, pkt->msg, pkt->hdr.sz, 
					pkt->flags, pkt->timeout);
		else
			err=inet_recv(pkt->sk, pkt->msg, pkt->hdr.sz, 
					pkt->flags);

		if(err != pkt->hdr.sz) {
			debug(DBG_NOISE, RED(ERROR_MSG) "Cannot recv the "
					"pkt's body", ERROR_FUNC);
			return -1;
		}
	}

	return err;
}

ssize_t pkt_recv(PACKET *pkt)
{
	ssize_t err=-1;

	switch(pkt->sk_type) {
		case SKT_UDP:
		case SKT_BCAST:
			err=pkt_recv_udp(pkt);
			break;

		case SKT_TCP:
			err=pkt_recv_tcp(pkt);
			break;

		default:
			fatal("Unkown socket_type. Something's very wrong!! Be aware");
			break;
	}

	/* let's finish it */
	pkt_unpack(pkt);

	return err;
}

int pkt_tcp_connect(inet_prefix *host, short port, interface *dev)
{
	int sk;
	PACKET pkt;
	const char *ntop;
	ssize_t err;

	ntop=inet_to_str(*host);
	setzero(&pkt, sizeof(PACKET));
	
	if((sk=new_tcp_conn(host, port, dev?dev->dev_name:0))==-1)
		goto finish;
	
	/*
	 * Now we receive the first pkt from the server. 
	 * It is an ack. 
	 * Let's hope it isn't NEGATIVE (-_+)
	 */
	pkt_addsk(&pkt, host->family, sk, SKT_TCP);
	pkt.flags=MSG_WAITALL;
	pkt_addport(&pkt, port);

	if((err=pkt_recv(&pkt)) < 0) {
		error("Connection to %s failed: it wasn't possible to receive "
				"the ACK", ntop);
		ERROR_FINISH(sk, -1, finish);
	}
	
	/* ...Last famous words */
	if(pkt.hdr.op != ACK_AFFERMATIVE) {
		u_char err;
		
		memcpy(&err, pkt.msg, pkt.hdr.sz);
		error("Cannot connect to %s:%d: %s", 
				ntop, port, rq_strerror(err));
		ERROR_FINISH(sk, -1, finish);
	}
	
finish:
	pkt_free(&pkt, 0);
	return sk;
}

void pkt_fill_hdr(pkt_hdr *hdr, u_char flags, int id, rq_t op, size_t sz)
{
	hdr->ntk_id[0]='n';
	hdr->ntk_id[1]='t';
	hdr->ntk_id[2]='k';

	hdr->id	   = !id ? xrand_fast() : id;
	hdr->flags = flags;
	hdr->op	   = op;
	hdr->sz	   = sz;
}



/*\
 *
 *	* * *  Pkt_op_table functions  * * *
 *
 * Functions used to manipulate the `pkt_op_table'. 
 * See pkts.h for more info.
 *
\*/

/*
 * pktop_hash_cmp
 */
int pktop_hash_cmp(const void *a, const void *b)
{
	struct pkt_op_table *ai=(struct pkt_op_table *)a, 
			    *bi=(struct pkt_op_table *)b;

	return (ai->rq_hash > bi->rq_hash) - (ai->rq_hash < bi->rq_hash);
}

/*
 * pktop_sort_table
 *
 * Sorts the `pkt_op_tbl' table in ascending order, comparing the
 * pkt_op_tbl->rq_hash values.
 */
void pktop_sort_table(void)
{
	qsort(pkt_op_tbl, pkt_op_tbl_counter,
			sizeof(struct pkt_op_table), pktop_hash_cmp);
	pkt_op_tbl_sorted=1;
}

/*
 * pktop_bsearch_rq
 *
 * Performs a bsearch() on `pkt_op_tbl' searching for the matching `rq_hash'.
 * If the search has a positive result, a pointer to the found struct is
 * returned, otherwise 0 will be the return value.
 */
struct pkt_op_table *
pktop_bsearch_rq(rq_t rq_hash)
{
	struct pkt_op_table pot_tmp = { .rq_hash = rq_hash };

	if(!pkt_op_tbl_sorted)
		pktop_sort_table();

	return (struct pkt_op_table *)
		bsearch(&pot_tmp, pkt_op_tbl, pkt_op_tbl_counter,
			sizeof(struct pkt_op_table), pktop_hash_cmp);
}

/* 
 * pktop_add_op
 *
 * Associate the `exec_f' function to the `rq_hash' request, in this way, when
 * pkt_exec() will execute `exec_f' when it receive a request equal to `rq_hash'.
 * The argument given to `exec_f' is a PACKET struct which contains the
 * received packet.
 *
 * `rq_hash' is the request/reply hash. WARNING: no check is made if `rq_hash'
 * corresponds to a valid registered request or reply. Only if `rq_hash' has 
 * been already registered or if a hash collision occurs, fatal() is immediately
 * called.
 *
 * `sk_type' specifies the type of the socket and can be any of SKT_TCP,
 * SKT_UDP, SKT_BCAST.
 * 
 * `port' is just the port where the request will be received/sent.
 *
 * See the description of the pkt_op_table in pkts.h for more infos.
 */
void pktop_add_op(rq_t rq_hash, char sk_type, u_short port, 
		int (*exec_f)(PACKET pkt))
{
	int i;

	pkt_op_tbl=xrealloc(pkt_op_tbl, 
			    (pkt_op_tbl_counter+1)*sizeof(struct pkt_op_table));

	for(i=0; i<pkt_op_tbl_counter; i++)
		if(pkt_op_tbl[i].rq_hash == rq_hash)
			fatal(ERROR_MSG 
				"The \"%s\" request/reply has been already"
				"registered with pkt_add_op()."
				"If it hasn't, then its hash is colliding with"
				"another request/reply (VERY unlikely)",
			      ERROR_POS, rq_rqerr_to_str(rq_hash));

	pkt_op_tbl[pkt_op_tbl_counter].rq_hash   = rq_hash;
	pkt_op_tbl[pkt_op_tbl_counter].sk_type   = sk_type;
	pkt_op_tbl[pkt_op_tbl_counter].port	 = port;
	pkt_op_tbl[pkt_op_tbl_counter].exec_func = exec_f;

	pkt_op_tbl_counter++;
	pkt_op_tbl_sorted=0;
}

/*
 * pktop_del_op
 *
 * Removes the `rq_hash' request (or reply) from the pkt_op_tbl table.
 */
void pktop_del_op(rq_t rq_hash)
{
	struct pkt_op_table *pot=0;
	int idx;

	if(!(pot=pktop_bsearch_rq(rq_hash)))
		return;

	idx=((char *)pot-(char *)pkt_op_tbl)/sizeof(struct pkt_op_table);
	if(idx < pkt_op_tbl_counter-1)
		/* Shifts all the succesive elements of `idx', in this way,
		 * the order of the array isn't changed */
		memmove(&pkt_op_tbl[idx], 
			&pkt_op_tbl[idx+1],
			sizeof(struct pkt_op_table)*(pkt_op_tbl_counter-idx-2));
	pkt_op_tbl_counter--;
	pkt_op_tbl=xrealloc(pkt_op_tbl, pkt_op_tbl_counter*sizeof(struct pkt_op_table));
}


/*\
 *
 * 	* * *  Send_rq, pkt_forward, pkt_err, pkt_exec  * * *
 *
 * Functions to send, forward and exec a packet.
 *
\*/


/*
 * 		            pkt_send_rq
 *		          ===============
 *
 * This functions sends the `rq_hash' request, with the id set 
 * to `rq_id', to `pkt->to'.
 * If `rq_id' is zero, a random id will be chosen.
 *
 *
 * Outgoing packet (`pkt')
 * -----------------------
 *
 * If `pkt->sk' is non zero, it will be used to send the request.
 * If `pkt->sk' is 0, a new socket will be created and connected to `pkt->to'.
 * The new socket will be stored in `pkt->sk'.
 *
 * If `pkt->hdr.sz` is > 0, the message contained in `pkt->msg' will be
 * included in the outgoing packet, otherwise `pkt->msg' will be set to 0.
 * It is possible to specify how the message will be included in the pkt:
 *	 If PKT_COMPRESSED is set in `pkt'->pkt_flags, `pkt'->msg 
 *	 will be compressed only if its size is > PKT_COMPRESS_THRESHOLD.
 *
 * If `pkt'->dev is not null and the PKT_BIND_DEV flag is set in
 * `pkt'->pkt_flags, the socket will be bound to the outgoing/ingoing packet to
 * the device named `pkt'->dev->dev_name.
 *
 *
 * Reply packet (`rpkt')
 * ---------------------
 *
 * The reply sent by the remote host to the request which has been sent in the
 * `Outgoing packet' is the `Reply packet'.
 *
 * If `rpkt' is not null, the reply packet will be received and stored 
 * in `rpkt'.
 *
 * If `check_ack' is set, pkt_send_rq() checks if the reply pkt contains an 
 * ACK_NEGATIVE error code and checks also if its the reply id matches the id
 * and the rq_hash of the `Outgoing pkt'; if the test fails it gives an 
 * appropriate error and returns.
 * Instead, if `check_ack' is zero, no check is made on the received packet.
 *
 * If `pkt'->hdr.flags has the ASYNC_REPLY set, the `rpkt' will be received with
 * the pkt_queue method, in this case, if `rpkt'->from is set to a valid ip, it 
 * will be used to check the sender ip of the reply pkt.
 *
 *
 * Returned value
 * --------------
 *
 * On failure a negative value is returned, otherwise 0 will be the returned
 * value.
 * The error values are defined in pkts.h.
 */
int pkt_send_rq(PACKET *pkt, int pkt_flags, rq_t rq_hash, int rq_id, re_t re_hash,
		int check_ack, PACKET *rpkt)
{
	struct pkt_op_table *pot=0;
	request *rq_h;

	ssize_t err;
	int ret=0;
	const char *ntop=0;
	const u_char *rq_str=0, *re_str=0;
	inet_prefix *wanted_from=0;


	if(!(rq_h=rq_get_rqstruct(rq_hash))) {
		error(ERROR_MSG "\"0x%x\" request/reply is not valid!", 
				ERROR_POS, rq_hash);
		return SEND_RQ_ERR_RQ;
	}
	rq_str = (u_char *) rq_h->name;
	pot = pktop_bsearch_rq(rq_hash);

	ntop=inet_to_str(pkt->to);

	/* * * the request building process * * */
	if(check_ack)
		pkt->hdr.flags|=SEND_ACK;
	
	pkt_fill_hdr(&pkt->hdr, pkt->hdr.flags, rq_id, rq_hash, pkt->hdr.sz);
	if(!pkt->hdr.sz)
		pkt->msg=0;

	if(!pkt->port) {
		if(pot && !pot->port && !pkt->sk) {
			error("pkt_send_rq: The rq %s doesn't have an associated "
					"port.", rq_str);
			ERROR_FINISH(ret, SEND_RQ_ERR_PORT, finish);
		}
		pkt_addport(pkt, pot->port);
	}

	/* If the PKT_BIND_DEV flag is set we can use pkt->dev */
	pkt->dev = (pkt->pkt_flags & PKT_BIND_DEV) ? pkt->dev : 0;

	if(!pkt->sk_type)
		pkt->sk_type=pot->sk_type;

	if(!pkt->sk) {
		if(!pkt->to.family || !pkt->to.len) {
			error("pkt->to isn't set. I can't create the new connection");
			ERROR_FINISH(ret, SEND_RQ_ERR_TO, finish);
		}
		
		if(pkt->sk_type==SKT_TCP)
			pkt->sk=pkt_tcp_connect(&pkt->to, pkt->port, pkt->dev);
		else if(pkt->sk_type==SKT_UDP)
			pkt->sk=new_udp_conn(&pkt->to, pkt->port, pkt->dev->dev_name);
		else if(pkt->sk_type==SKT_BCAST) {
			if(!pkt->dev)
				fatal(RED(ERROR_MSG) "cannot broadcast the packet: "
						"device not specified", ERROR_FUNC);
			pkt->sk=new_bcast_conn(&pkt->to, pkt->port, pkt->dev->dev_idx);
		} else
			fatal("Unkown socket_type. Something's very wrong!! Be aware");

		if(pkt->sk==-1) {
			error("Couldn't connect to %s to launch the %s request", ntop, rq_str);
			ERROR_FINISH(ret, SEND_RQ_ERR_CONNECT, finish);
		}
	}

	/* Set the LOWDELAY TOS if necessary */
	if(pkt->pkt_flags & PKT_SET_LOWDELAY)
		set_tos_sk(pkt->sk, 1);

	if(pkt->pkt_flags & PKT_NONBLOCK)
		set_nonblock_sk(pkt->sk);

	/*Let's send the request*/
	err=pkt_send(pkt);
	if(err==-1) {
		error("Cannot send the %s request to %s:%d.", rq_str, ntop, pkt->port);
		ERROR_FINISH(ret, SEND_RQ_ERR_SEND, finish);
	}

	/*
	 *  * * the reply * * 
	 */
	if(rpkt) {
		if(rpkt->from.data[0] && rpkt->from.len) {
			wanted_from=&rpkt->from;
			ntop=inet_to_str(rpkt->from);
		}

		setzero(rpkt, sizeof(PACKET));
		pkt_addport(rpkt, pkt->port);
		pkt_addsk(rpkt, pkt->to.family, pkt->sk, pkt->sk_type);
		rpkt->flags=MSG_WAITALL;
		pkt_addtimeout(rpkt, pkt->timeout, pkt->pkt_flags&PKT_RECV_TIMEOUT,
				pkt->pkt_flags&PKT_SEND_TIMEOUT);
		if(pkt->pkt_flags & PKT_COMPRESSED)
			pkt_addcompress(rpkt);
		
		debug(DBG_NOISE, "Receiving reply for the %s request"
				" (id 0x%x)", rq_str, pkt->hdr.id);

		if(pkt->hdr.flags & ASYNC_REPLY) {
			pkt_queue *pq;
			/* Receive the pkt in the async way */
			err=pkt_q_wait_recv(pkt->hdr.id, wanted_from, rpkt, &pq);
			pkt_q_del(pq, 0);
		} else {
			if(pkt->sk_type==SKT_UDP) {
				inet_copy(&rpkt->from, &pkt->to);
				ntop=inet_to_str(rpkt->from);
			}

			/* Receive the pkt in the standard way */
			err=pkt_recv(rpkt);
		}

		if(err==-1) {
			error("Error while receving the reply for the %s request"
					" from %s.", rq_str, ntop);
			ERROR_FINISH(ret, SEND_RQ_ERR_RECV, finish);
		}

		if((rpkt->hdr.op == ACK_NEGATIVE) && check_ack) {
			rqerr_t err_ack;

			memcpy(&err_ack, rpkt->msg, sizeof(rqerr_t));

			error("%s failed. The node %s replied: %s", rq_str, ntop, 
					rq_strerror(err_ack));
			ERROR_FINISH(ret, SEND_RQ_ERR_REPLY, finish);

		} else if(rpkt->hdr.op != re_hash && check_ack) {

			error("The node %s replied %s but we asked %s!", ntop, 
					re_to_str(rpkt->hdr.op), re_str);
			ERROR_FINISH(ret, SEND_RQ_ERR_RECVOP, finish);
		}

		if(check_ack && rpkt->hdr.id != pkt->hdr.id) {
			error("The id (0x%x) of the reply (%s) doesn't match the"
					" id of our request (0x%x)", rpkt->hdr.id,
					re_str, pkt->hdr.id);
			ERROR_FINISH(ret, SEND_RQ_ERR_RECVID, finish);
		}
	}

finish:
	return ret;
}

/*
 * pkt_forward
 *
 * forwards the received packet `rpkt' to `to'.
 */
int pkt_forward(PACKET rpkt, inet_prefix to)
{
	int err;

	rpkt.sk=0; /* create a new connection */
	pkt_addto(&rpkt, &to);
	
	err=pkt_send_rq(&rpkt, 0, rpkt.hdr.op, rpkt.hdr.id, 0, 0, 0);
	if(!err)
		inet_close(&rpkt.sk);

	return err;
}

/* 
 * pkt_err
 *
 * Sends back to `pkt.from' an error pkt, with ACK_NEGATIVE, 
 * containing the given `err' error code.
 *
 * If `free_pkt' is not 0, `pkt' will be freed.
 *
 * On error a negative value is returned.
 */
int pkt_err(PACKET pkt, rqerr_t err, int free_pkt)
{
	char *msg;
	u_char flags=0;
	
	pkt_addto(&pkt, &pkt.from);
	if(pkt.hdr.flags & ASYNC_REPLY) {
		flags|=ASYNC_REPLIED;
		pkt.sk=0;
	}

	/* It's useless to compress this pkt */
	pkt.pkt_flags&=~PKT_COMPRESSED;

	pkt_fill_hdr(&pkt.hdr, flags, pkt.hdr.id, ACK_NEGATIVE, sizeof(rqerr_t));
	
	pkt.msg=msg=xmalloc(sizeof(rqerr_t));
	memcpy(msg, &err, sizeof(rqerr_t));
		
	err=pkt_send_rq(&pkt, 0, ACK_NEGATIVE, pkt.hdr.id, 0, 0, 0);

	if(pkt.hdr.flags & ASYNC_REPLY)
		pkt_free(&pkt, 1);
	else
		pkt_free(&pkt, 0);
	return err;
}


/*
 * pkt_exec
 *
 * It "executes" the received `pkt' passing it to the function which has been
 * associated to the `pkt'.hdr.op request.
 */
int pkt_exec(PACKET pkt)
{
	struct pkt_op_table *pot=0;
	request *rq=0;

	const char *ntop;
	const u_char *op_str;
	int (*exec_f)(PACKET pkt)=0;
	int err=0;


	if(!(rq=rq_get_rqstruct(pkt.hdr.op))) {
		debug(DBG_SOFT, "Dropped pkt from %s: bad op value", 
				inet_to_str(pkt.from));
		return -1;	/* bad op */
	} else
		op_str=rq->name;

	if(op_filter_test(pkt.hdr.op)) {
		/* Drop the pkt, `pkt.hdr.op' has been filtered */ 
#ifdef DEBUG
		ntop=inet_to_str(pkt.from);
		debug(DBG_INSANE, "FILTERED %s from %s, id 0x%x", op_str, ntop,
				pkt.hdr.id);
#endif
		return err;
	}
		
	/* Call the function associated to `pkt.hdr.op' */
	if((pot = pktop_bsearch_rq(pkt.hdr.op)))
			exec_f = pot->exec_func;
#ifdef DEBUG
	if(pkt.hdr.op != rq_hash_name("ECHO_ME") && 
			pkt.hdr.op != rq_hash_name("ECHO_REPLY")) {
		ntop=inet_to_str(pkt.from);
		debug(DBG_INSANE, "Received %s from %s, id 0x%x", op_str, ntop,
				pkt.hdr.id);
	}
#endif

	if(exec_f)
		err=(*exec_f)(pkt);
	else if(pkt_q_counter) {
		debug(DBG_INSANE, "pkt_exec: %s Async reply, id 0x%x", op_str,
				pkt.hdr.id);
		/* 
		 * There isn't a function to handle this pkt, so maybe it is
		 * an async reply
		 */
		pkt_q_add_pkt(pkt);
	}

	return err;
}


/*\
 *
 * 	* * *  Pkt queue functions  * * *
 *
\*/

pthread_attr_t wait_and_unlock_attr;
void pkt_queue_init(void)
{
	pkt_q=(pkt_queue *)clist_init(&pkt_q_counter);

	pthread_attr_init(&wait_and_unlock_attr);
        pthread_attr_setdetachstate(&wait_and_unlock_attr, PTHREAD_CREATE_DETACHED);
}

void pkt_queue_close(void)
{
	pkt_queue *pq=pkt_q, *next;
	if(pkt_q_counter)
		list_safe_for(pq, next)
			pkt_q_del(pq, 1);
	pthread_attr_destroy(&wait_and_unlock_attr);
}

/* 
 * wait_and_unlock
 * 
 * It waits REQUEST_TIMEOUT seconds, then it unlocks `pq'->mtx.
 * This prevents the dead lock in pkt_q_wait_recv()
 */
void *wait_and_unlock(void *m)
{
	pkt_queue *pq, **pq_ptr;
	int i;

	pq_ptr=(pkt_queue **)m;
	pq=*pq_ptr;
	if(!pq)
		return 0;

	for(i=0; i<REQUEST_TIMEOUT; i++) {
		sleep(1);
		if(!(*pq_ptr) || (pq->flags & PKT_Q_PKT_RECEIVED) ||
				!(pq->flags & PKT_Q_MTX_LOCKED) ||
				pthread_mutex_trylock(&pq->mtx) != EBUSY)
			break;
	}

	if(!(*pq_ptr) || (pq->flags & PKT_Q_PKT_RECEIVED) ||
			!(pq->flags & PKT_Q_MTX_LOCKED) ||
			pthread_mutex_trylock(&pq->mtx) != EBUSY)
		goto finish;

	debug(DBG_INSANE, "pq->pkt.hdr.id: 0x%x Timeoutted. mtx: 0x%X", 
			pq->pkt.hdr.id, (int)&pq->mtx);
	pthread_mutex_unlock(&pq->mtx);
	pq->flags|=PKT_Q_TIMEOUT;
	
finish:
	if(pq_ptr)
		xfree(pq_ptr);
	return 0;
}

/*
 * pkt_q_wait_recv
 *
 * adds a new struct in pkt_q and waits REQUEST_TIMEOUT
 * seconds until a reply with an id equal to `id' is received.
 * If `from' is not null, the sender ip of the reply is considered too.
 * The received reply pkt is copied in `rpkt' (if `rpkt' isn't null).
 * In `ret_pq' is stored the address of the pkt_queue struct that 
 * corresponds to `rpkt'.
 * After the use of this function pkt_q_del() must be called.
 * On error -1 is returned.
 */
int pkt_q_wait_recv(int id, inet_prefix *from, PACKET *rpkt, pkt_queue **ret_pq)
{
	pthread_t thread;
	pkt_queue *pq, **pq_ptr;

	
	pq=xzalloc(sizeof(pkt_queue));
	pq_ptr=xmalloc(sizeof(pkt_queue *));
	*pq_ptr=pq;
	
	pthread_mutex_init(&pq->mtx, 0);
	pq->flags|=PKT_Q_MTX_LOCKED;
	*ret_pq=pq;
	
	if(!pkt_q_counter)
		pkt_queue_init();

	pq->pkt.hdr.id=id;
	if(from) {
		debug(DBG_INSANE, "0x%x wanted_rfrom %s activated", id, 
				inet_to_str(*from));
		inet_copy(&pq->pkt.from, from);
		pq->flags|=PKT_Q_CHECK_FROM;
	}

	clist_add(&pkt_q, &pkt_q_counter, pq);

	/* Be sure to unlock me after the timeout */
	pthread_create(&thread, &wait_and_unlock_attr, wait_and_unlock, 
			(void *)pq_ptr);

	if(pq->flags & PKT_Q_MTX_LOCKED) {
		debug(DBG_INSANE, "pkt_q_wait_recv: Locking 0x%x!", (int)&pq->mtx);

		/* Freeze! */
		pthread_mutex_lock(&pq->mtx);
		pthread_mutex_lock(&pq->mtx);
	}

	debug(DBG_INSANE, "We've been unlocked: timeout %d", (pq->flags & PKT_Q_TIMEOUT));
	if(pq->flags & PKT_Q_TIMEOUT)
		return -1;

	if(rpkt)
		pkt_copy(rpkt, &pq->pkt);

	/* When *pq_ptr is set to 0, the wait_and_unlock thread exits */
	*pq_ptr=0;

	return 0;
}

/*
 * pkt_q_add_pkt: Copy the reply pkt in the struct of pkt_q which has the same
 * hdr.id, then unlock the mutex of the pkt_q struct.
 * If the struct in pkt_q isn't found, -1 is returned.
 */
int pkt_q_add_pkt(PACKET pkt)
{
	pkt_queue *pq=pkt_q, *next=0;
	int ret=-1;
	
	list_safe_for(pq, next) {
		debug(DBG_INSANE, "pkt_q_add_pkt: %d == %d. data[0]: %d, async replied: %d",
				pq->pkt.hdr.id, pkt.hdr.id, pq->pkt.from.data[0],
				(pkt.hdr.flags & ASYNC_REPLIED));
		if(pq->pkt.hdr.id == pkt.hdr.id) {
			if(pq->pkt.from.data[0] && (pq->flags & PKT_Q_CHECK_FROM) &&
					memcmp(pq->pkt.from.data, pkt.from.data, MAX_IP_SZ))
					continue; /* The wanted from ip and the
						     real from ip don't match */
			if(!(pkt.hdr.flags & ASYNC_REPLIED))
				continue;

			pkt_copy(&pq->pkt, &pkt);
			
			/* Now it's possible to read the reply,
			 * pkt_q_wait_recv() is now hot again */
			while(pthread_mutex_trylock(&pq->mtx) != EBUSY)
				usleep(5000);
			debug(DBG_INSANE, "pkt_q_add_pkt: Unlocking 0x%X ", (int)&pq->mtx);
			pq->flags&=~PKT_Q_MTX_LOCKED & ~PKT_Q_TIMEOUT;
			pq->flags|=PKT_Q_PKT_RECEIVED;
			pthread_mutex_unlock(&pq->mtx);
			pthread_mutex_unlock(&pq->mtx);
			ret=0;
		}
	}

	return ret;
}

/*
 * pkt_q_del: Deletes `pq' from the pkt_q llist and frees the `pq' struct. The 
 * `pq'->pkt is also freed and the pq->pkt.sk socket is closed if `close_socket' 
 * is non zero.
 */
void pkt_q_del(pkt_queue *pq, int close_socket)
{
	pthread_mutex_unlock(&pq->mtx);
	pthread_mutex_destroy(&pq->mtx);

	pkt_free(&pq->pkt, close_socket);
	clist_del(&pkt_q, &pkt_q_counter, pq);
}
