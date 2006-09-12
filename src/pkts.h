/* This file is part of Netsukuku system
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
 */

#ifndef PKTS_H
#define PKTS_H

#include "if.h"
#include "request.h"
#include "llist.c"

/*\
 *
 *			Packet API
 *		      ==============
 *
 * Sending and receiving a packet
 * ------------------------------
 *
 * It's simple just use pkt_send_rq(). See its description in pkts.c.
 * Here it is an example:
 * 		
 * 		PACKET pkt, rpkt;
 *
 * 		setzero(&pkt, sizeof(PACKET));
 * 		setzero(&rpkt, sizeof(PACKET));
 *
 * 		(* Fill the header of the packet *)
 *        	pkt_fill_hdr(&pkt.hdr, 0, 0, FOO_GET_NEW_MAP, 512);
 *       	 	pkt_addto(&pkt, &rfrom);
 *        	pkt_addsk(&pkt, my_family, 0, SKT_TCP);
 *        	pkt_addport(&pkt, ntk_tcp_port);
 *
 *	        (* Compress the packet *)
 *        	pkt_addcompress(&pkt);  
 *
 *		(* Put in pkt.msg the body of the packet *)
 *		pkt.msg=xmalloc(512);
 *		get_rand_bytes(pkt.msg, 512);
 *
 *		(* Send it and expect to receive the FOO_PUT_NEW_MAP reply *)
 *		err=pkt_send_rq(&pkt, 0, FOO_GET_NEW_MAP, 0,
 *		                        FOO_PUT_NEW_MAP, 1, &rpkt);
 *		if(err < 0)
 *			... something went bad ...
 *
 *		... do something with `rpkt' ...
 *
 *		pkt_free(&pkt, 0);
 *		pkt_free(&rpkt, 0);
 *
 * To know all the various modifier of the packet, just see the exported
 * functions which are listed at the end of this file.
 *
 * Note: if you just need to forward a received pkt use pkt_forward().
 * 
 * Sending an Error Reply
 * ----------------------
 * 
 * Let's suppose you've received a packet with pkt_send_rq() (see above).
 * The best way to send back an error as a reply is using the pkt_err()
 * function. Its format is 
 * 
 * 	int pkt_err(PACKET pkt, rqerr_t err, int free_pkt)
 *
 * where `pkt' is the received pkt, and error is a Request Error which has
 * been previously registered with rqerr_add_error() (see request.h).
 * pkt_err() will send to `pkt'.from the specified `err' request error.
 *
 *
 * Receiving a request
 * -------------------
 *
 * Suppose the module Foo, has registered the FOO_GET_NEW_MAP request with
 * rq_add_request() (see request.h).
 *
 * In order to be able to receive directly the FOO_GET_NEW_MAP requests which
 * have been received by the local host, Foo has to add FOO_GET_NEW_MAP in the
 * `pkt_op_tbl'. This is accomplished using the pktop_add_op() function.
 *
 * When the module Foo will be de-initialized, it will call pktop_del_op().
 * 
 * Example:
 * 		int rcv_get_new_map(PACKET pkt)
 * 		{
 * 			... do something with pkt ...
 *
 * 			pkt_send_rq(... send the reply ...);
 * 		}
 *
 * 		void init_foo(void)
 * 		{
 * 			...
 * 			RQ_ADD_REQUEST( FOO_GET_NEW_MAP, 0 );
 * 			...
 * 			pktop_add_op(FOO_GET_NEW_MAP, SKT_TCP, 
 * 					ntk_tcp_port, rcv_get_new_map);
 *	 	}
 *
 *	 	void close_foo(void)
 *	 	{
 *	 		...
 *	 		rq_del_request(FOO_GET_NEW_MAP);
 *			...
 *	 		pktop_del_op(FOO_GET_NEW_MAP)
 *	 	}
 */
 
#define NETSUKUKU_ID		"ntk"
#define MAXMSGSZ		65536

/* ACK replies */
rq_t ACK_AFFERMATIVE,			/* Ack affermative. Everything is fine. */
     ACK_NEGATIVE;			/* The request has been rejected. 
					   The error is in the pkt's body */
/* request error */
re_t E_INVALID_PKT;


/*\
 *
 * Pkt's op definitions
 *
\*/

/* Pkt.sk_type */
#define SKT_TCP 		1
#define SKT_UDP			2
#define SKT_BCAST		3	/* UDP sent in broadcast */

/* 
 * Pkt.pkt_flags flags 
 */
#define PKT_BIND_DEV		1	/* Bind the pkt.sk socket to pkt.dev */
#define PKT_RECV_TIMEOUT	(1<<1)
#define PKT_SEND_TIMEOUT	(1<<2)
#define PKT_SET_LOWDELAY	(1<<3)
#define PKT_COMPRESSED		(1<<4)	/* If set the packet will be Z 
					   compressed before being sent */
#define PKT_KEEPALIVE		(1<<5)  /* Let the pkt.sk socket be alive */
#define PKT_NONBLOCK		(1<<6)	/* Socket must not block */

/* 
 * Pkt.hdr flags 
 */
#define SEND_ACK		1
#define BCAST_PKT		(1<<1)	/* In this pkt there is encapsulated a 
					 * broadcast/flood pkt. Woa */
#define HOOK_PKT		(1<<2)  /* All the pkts sent while hooking have
					 * this flag set      */
#define ASYNC_REPLY		(1<<3)	/* Tells the receiver to reply with a new 
					   connection. The reply pkt will be
					   handled by the pkt_queue. */
#define ASYNC_REPLIED		(1<<4)
#define LOOPBACK_PKT		(1<<5)  /* This is a packet destinated to me */
#define RESTRICTED_PKT		(1<<6)	/* Packet sent from a node in restricted 
					   mode */
#define COMPRESSED_PKT		(1<<7)  /* The whole packet is Z compressed */


/*
 * Broacast ptk's flags
 */
#define BCAST_TRACER_PKT	1	/*When a bcast is marked with this, it 
					  acts as a tracer_pkt ;)*/
#define BCAST_TRACER_BBLOCK	(1<<1)  /*When set, the tracer pkt carries also
					  bnodes blocks.*/
#define BCAST_TRACER_STARTERS	(1<<2)  /*Tracer pkt bound to the qspn starter 
					  continual group*/
#define QSPN_BNODE_CLOSED	(1<<3)	/*The last bnode, who forwarded this 
					  qspn pkt has all its links closed.*/
#define QSPN_BNODE_OPENED	(1<<4)

/* General defines */
#define PKT_MAX_MSG_SZ		1048576	/* bytes */
#define PKT_COMPRESS_LEVEL	Z_DEFAULT_COMPRESSION
#define PKT_COMPRESS_THRESHOLD	1024	/* If the flag PKT_COMPRESSED is set 
					   and hdr.sz > PKT_COMPRESS_THRESHOLD,
					   then compress the packet */

/*
 * pkt_hdr
 * -------
 *
 * The pkt_hdr is always put at the very beginning of any packets
 */
typedef struct
{
	char		ntk_id[3];
	int 		id;
	u_char		flags; 
	rq_t		op;
	size_t 		sz;		/* The size of the message */
	size_t		uncompress_sz;	/* The size of the decompressed packet. */
}_PACKED_  pkt_hdr;
INT_INFO pkt_hdr_iinfo = { 4, 
			   { INT_TYPE_32BIT, INT_TYPE_32BIT, INT_TYPE_32BIT,
			     INT_TYPE_32BIT },
			   { sizeof(char)*3, sizeof(char)*4+sizeof(int),
			     sizeof(char)*4+sizeof(int)*2,
			     sizeof(char)*4+sizeof(int)*2+sizeof(size_t) },
			   { 1, 1, 1, 1 }
			 };
#define PACKET_SZ(sz) (sizeof(pkt_hdr)+(sz))

/*
 * PACKET
 * ------
 *
 * this struct is used only to represent internally a packet, which
 * will be sent or received.
 */
typedef struct
{
	/* General informations of the packet */
	
	inet_prefix 	from;		/* The sender ip of this packet */
	inet_prefix 	to;		/* Where to send this packet */

	interface	*dev;		/* Device used to send/receive the 
					   packet. `sk' will be bound to it
					   if `dev' is not null and if the 
					   PKT_BIND_DEV flag is set in
					   `pkt_flags'. `dev' is a pointer
					   to a struct contained in the 
					   me.cur_ifs array. */
	
	int		family;
	int 		sk;
	char 		sk_type;
	u_short 	port;

	u_char		pkt_flags;	/*Flags for this PACKET*/
	int 		flags;		/*Flags used by send/recv*/

	u_int		timeout;	/*After `timeout' seconds give up the
					  send/recv of the packet. 
					  The PKT_[RECV/SEND]_TIMEOUT flags are
					  used to determine its scope (send, 
					  recv or both).*/

	/* Body of the packet */
	pkt_hdr 	hdr;
	char 		*msg;
} PACKET;
	
/*Broadcast packet*/
typedef struct
{
	u_char		g_node;		/*The gnode the brdcast_pkt is restricted to*/
	u_char 		level;		/*The level of the g_node*/
	u_char	 	gttl;		/*Gnode ttl: How many gnodes the packet
					  can traverse*/
	u_char	 	sub_id;		/*The sub_id is the node who sent the pkt,
					  but is only used by the qspn_open*/
	size_t 		sz;		/*Sizeof(the pkt)*/
	char 		flags;		/*Various flags*/
}_PACKED_ brdcast_hdr;

INT_INFO brdcast_hdr_iinfo = { 1, { INT_TYPE_32BIT }, { sizeof(char)*4 }, { 1 } };
#define BRDCAST_SZ(pkt_sz) 	(sizeof(brdcast_hdr)+(pkt_sz))
#define BRDCAST_HDR_PTR(msg)	((brdcast_hdr *)(msg))


/* 
 * pkt_op_table
 * ------------
 *
 * In this table, each request or reply is associated with an `exec_func'.
 * When pkt_exec() will receive a pkt which has the same request/reply id, it
 * will call the `exec_func()' function, passing to it the received pkt.
 *
 * Each request is also associated with its specific socket type (udp, tcp, 
 * bcast) in `sk_type', and with the `port' where the pkt will be sent or
 * received.
 * `sk_type' can be SKT_TCP, SKT_UDP or SKT_BCAST.
 *
 * The table is kept ordered with qsort(), in this way it is possible to do a
 * fast bsearch() on it.
 */
struct pkt_op_table {
	rq_t		rq_hash;

	char 		sk_type;
	u_short 	port;

	void 		*exec_func;
};

/* pkt_queue's flags */
#define PKT_Q_MTX_LOCKED	1		/* We are waiting the reply */
#define PKT_Q_PKT_RECEIVED	(1<<1)		/* The reply was received */
#define PKT_Q_TIMEOUT		(1<<2)		/* None replied ._, */
#define PKT_Q_CHECK_FROM	(1<<3)		/* Check the from ip while
						   receiving the async pkt */

/*
 * pkt_queue
 * ---------
 *
 * The pkt_queue is used when a reply will be received with a completely new 
 * connection. 
 * This is what happens when we want to send out the packet `pkt' and we hope
 * to receive an ASYNC reply:
 * 	
 * 	* The pkt.hdr.flags is ORed with ASYNC_REPLY
 *
 * 	* A new struct is added in the pkt_q linked list:
 * 		* pkt_q->pkt.hdr.id is set to the id of the outgoing pkt
 * 		* pkt_q->pkt.hdr.op is set to the waited reply op.
 *
 * 	* The function wait_and_unlock() it's started as a new thread and 
 * 	  the request is sent
 *
 * 	* to receive the reply, wait_and_unlock() locks twice pkt_q->`mtx'.
 * 	  The thread is now freezed.
 *
 * 	* The reply is received by pkt_exec() which passes the pkt to the 
 * 	  function pkt_q_add_pkt().
 * 	  	* pkt_q_add_pkt() searches in the pkt_q llist a struct 
 * 	  	  which has the same pkt.hdr.id of the received pkt. 
 *
 * 	  	* The reply pkt is copied in the found struct
 *
 * 	  	* pkt_q->`mtx' is unlocked.
 *
 * 	* wait_and_unlock() can now continue to read the reply and 
 * 	  unlocks `mtx'.
 *
 * Note that the reply pkt must have the ASYNC_REPLIED flag set 
 * in pkt.hdr.flags.
 */
struct pkt_queue {
	LLIST_HDR	(struct pkt_queue);

	PACKET 		pkt;
	pthread_mutex_t mtx;

	char 		flags;
};
typedef  struct pkt_queue pkt_queue;

pkt_queue *pkt_q;
int pkt_q_counter;

/*Functions' declarations*/
void pkts_init(interface *ifs, int ifs_n, int queue_init);
	
void pkt_addfrom(PACKET *pkt, inet_prefix *from);
void pkt_addto(PACKET *pkt, inet_prefix *to);
void pkt_add_dev(PACKET *pkt, interface *dev, int bind_the_socket);
void pkt_addsk(PACKET *pkt, int family, int sk, int sk_type);
void pkt_addport(PACKET *pkt, u_short port);
void pkt_addflags(PACKET *pkt, int flags);
void pkt_addtimeout(PACKET *pkt, u_int timeout, int recv, int send);
void pkt_addcompress(PACKET *pkt);
void pkt_addlowdelay(PACKET *pkt);
void pkt_addnonblock(PACKET *pkt);
void pkt_addhdr(PACKET *pkt, pkt_hdr *hdr);
void pkt_addmsg(PACKET *pkt, char *msg);
void pkt_copy(PACKET *dst, PACKET *src);
void pkt_clear(PACKET *pkt);

void pkt_free(PACKET *pkt, int close_socket);
char *pkt_pack(PACKET *pkt);

int pkt_verify_hdr(PACKET pkt);
ssize_t pkt_send(PACKET *pkt);
ssize_t pkt_recv(PACKET *pkt);
int pkt_tcp_connect(inet_prefix *host, short port, interface *dev);

void pkt_fill_hdr(pkt_hdr *hdr, u_char flags, int id, rq_t op, size_t sz);

#define SEND_RQ_ERR		-1
#define SEND_RQ_ERR_RQ		-2
#define SEND_RQ_ERR_RE		-3
#define SEND_RQ_ERR_PORT	-4
#define SEND_RQ_ERR_TO		-5
#define SEND_RQ_ERR_CONNECT	-6
#define SEND_RQ_ERR_SEND	-7
#define SEND_RQ_ERR_RECV	-8
#define SEND_RQ_ERR_RECVOP	-9
#define SEND_RQ_ERR_RECVID	-10
#define SEND_RQ_ERR_REPLY	-11
int pkt_send_rq(PACKET *pkt, int pkt_flags, rq_t rq_hash, int rq_id, re_t re_hash,
		int check_ack, PACKET *rpkt);

int pkt_forward(PACKET rpkt, inet_prefix to);
int pkt_err(PACKET pkt, rqerr_t err, int free_pkt);

void pktop_add_op(rq_t rq_hash, char sk_type, u_short port, 
		int (*exec_f)(PACKET pkt));
void pktop_del_op(rq_t rq_hash);
int pkt_exec(PACKET pkt);

void pkt_queue_init(void);
void pkt_queue_close(void);
int pkt_q_wait_recv(int id, inet_prefix *from, PACKET *rpkt, pkt_queue **ret_pq);
int pkt_q_add_pkt(PACKET pkt);
void pkt_q_del(pkt_queue *pq, int close_socket);

#endif /*PKTS_H*/
