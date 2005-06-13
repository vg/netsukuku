/* This file is part of Netsukuku system
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#define NETSUKUKU_ID		"ntk"
#define MAXMSGSZ		65536

/* 
 * Pkt's op definitions:
 * The request and replies are in request.h
 */

/* Pkt's sk_type */
#define SKT_TCP 		1
#define SKT_UDP			2
#define SKT_UDP_RADAR		3
#define SKT_BCAST		4
#define SKT_BCAST_RADAR		5

/* Pkt's flags */
#define SEND_ACK		1
#define BCAST_PKT		(1<<1)	/* In this pkt there is encapsulated a 
					 * broadcast pkt. Woa 
					 */
#define HOOK_PKT		(1<<2)  /* All the pkts sent while hooking have
					 * this flag set 
					 */

/* Broacast ptk's flags */
#define BCAST_TRACER_PKT	1	/*When a bcast is marked with this, it 
					  acts as a tracer_pkt ;)*/
#define BCAST_TRACER_BBLOCK	(1<<1)  /*When set, the tracer pkt carries also
					  bnodes blocks.*/
#define BCAST_TRACER_STARTERS	(1<<2)  /*Tracer pkt bound to the qspn starter 
					  continual group*/
#define QSPN_BNODE_CLOSED	(1<<3)	/*The last bnode, who forwarded this 
					  qspn pkt has all its links closed.*/
#define QSPN_BNODE_OPENED	(1<<4)


typedef struct
{
	char		ntk_id[3];
	int 		id;
	u_char		flags; 
	u_char 		op;
	size_t 		sz;
}_PACKED_  pkt_hdr;
#define PACKET_SZ(sz) (sizeof(pkt_hdr)+(sz))		

typedef struct
{
	inet_prefix 	from;		
	inet_prefix 	to;
	int		family;
	int 		sk;
	char 		sk_type;
	u_short 	port;
	int 		flags;
	
	pkt_hdr 	hdr;
	char 		*msg;
}PACKET;
	
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
#define BRDCAST_SZ(pkt_sz) (sizeof(brdcast_hdr)+(pkt_sz))

/***The nodeblock of the node*/
struct node_hdr
{
	struct sockaddr ip;		/*Ip of the node*/
	__u16 links;			/*Number of r_nodes*/
}_PACKED_;
struct rnode_chunk
{	
	struct sockaddr r_node;         /*Ip of the r_node*/
	struct timeval  rnode_t;	/*node <-> r_node time*/	
}_PACKED_;
#define NODEBLOCK_SZ(links) (sizeof(struct node_hdr)+sizeof((struct r_node)*(links)))


/*Functions' declarations*/
void pkt_addfrom(PACKET *pkt, inet_prefix *from);
void pkt_addto(PACKET *pkt, inet_prefix *to);
void pkt_addsk(PACKET *pkt, int family, int sk, int sk_type);
void pkt_addport(PACKET *pkt, u_short port);
void pkt_addflags(PACKET *pkt, int flags);
void pkt_addhdr(PACKET *pkt, pkt_hdr *hdr);
void pkt_addmsg(PACKET *pkt, char *msg);
void pkt_copy(PACKET *dst, PACKET *src);
void pkt_clear(PACKET *pkt);

void pkt_free(PACKET *pkt, int close_socket);
char *pkt_pack(PACKET *pkt);
PACKET *pkt_unpack(char *pkt);

int pkt_verify_hdr(PACKET pkt);
ssize_t pkt_send(PACKET *pkt);
ssize_t pkt_recv(PACKET *pkt);
int pkt_tcp_connect(inet_prefix *host, short port);

void pkt_fill_hdr(pkt_hdr *hdr, u_char flags, int id, u_char op, size_t sz);

int send_rq(PACKET *pkt, int flags, u_char rq, int rq_id, u_char re, int check_ack, PACKET *rpkt);
int pkt_err(PACKET pkt, u_char err);

int pkt_exec(PACKET pkt, int acpt_idx);
