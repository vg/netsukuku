/* This file is part of Netsukuku system
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

#define NETSUKUKU_ID		"ntk"
#define MAXMSGSZ		32768

/* 
 * Pkt's op definitions:
 * The request and replies are in request.h
 */

/* Pkt's sk_type */
#define SKT_TCP 	1
#define SKT_UDP		2
#define SKT_BCAST	3

/* Pkt's flags */
#define SEND_ACK		1
#define BCAST_PKT		(1<<1)	/*In this pkt there is encapsulated a 
					  broadcast pkt. Woa*/

/* Broacast ptk's flags */
#define BCAST_TRACER_PKT	1	/*When a bcast is marked with this, it 
					  acts as a tracer_pkt ;)*/
#define QSPN_NO_OPEN		(1<<1)	/*The qspn_close pkts with this flag set
					  will not propagate the qspn_open*/

typedef struct
{
	char ntk_id[3];
	int id;
	u_char op;
	size_t sz;
}pkt_hdr;
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
	u_short		g_node;		/*The gnode the brdcast_pkt is restricted to*/
	u_char 		level;		/*The level of the gnode*/
	inet_prefix 	g_ipstart;	/*The ipstart of the g_node in level*/
	u_short 	gttl;		/*Gnode ttl: How many gnodes the packet
					  can traverse*/
	u_short 	sub_id;		/*The sub_id is the node who sent the pkt,
					  but is only used by the qspn_open*/
	size_t 		sz;		/*Sizeof(the pkt)*/
	char 		flags;		/*Various flags*/
}brdcast_hdr;
#define BRDCAST_SZ(pkt_sz) (sizeof(brdcast_hdr)+(pkt_sz))

/***The nodeblock of the node*/
struct node_hdr
{
	struct sockaddr ip;		/*Ip of the node*/
	__u16 links;			/*Number of r_nodes*/
};
struct rnode_chunk
{	
	struct sockaddr r_node;         /*Ip of the r_node*/
	struct timeval  rnode_t;	/*node <-> r_node time*/	
};
#define NODEBLOCK_SZ(links) (sizeof(struct node_hdr)+sizeof((struct r_node)*(links)))


/*Functions' declarations*/
void pkt_addfrom(PACKET *pkt, inet_prefix *from);
void pkt_addto(PACKET *pkt, inet_prefix *to);
void pkt_addsk(PACKET *pkt, int family, int sk, int sk_type);
void pkt_addport(PACKET *pkt, u_short port);
void pkt_addflags(PACKET *pkt, int flags);
void pkt_addhdr(PACKET *pkt, pkt_hdr *hdr);
void pkt_addmsg(PACKET *pkt, char *msg);
void pkt_free(PACKET *pkt, int close_socket);
char *pkt_pack(PACKET *pkt);
PACKET *pkt_unpack(char *pkt);
int pkt_verify_hdr(PACKET pkt);
ssize_t pkt_send(PACKET *pkt);
ssize_t pkt_recv(PACKET *pkt);
int pkt_tcp_connect(inet_prefix *host, short port);
void pkt_fill_hdr(pkt_hdr *hdr, int id, u_char op, size_t sz);
int send_rq(PACKET *pkt, int flags, u_char rq, int rq_id, u_char re, int check_ack, PACKET *rpkt);
int pkt_err(PACKET pkt, u_char err);
int pkt_exec(PACKET pkt, int acpt_idx);
