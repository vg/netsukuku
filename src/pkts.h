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

#include <sys/time.h>
#include <sys/types.h>
#include <asm/types.h>
#include <linux/socket.h>
#include "inet.h"

#define NETSUKUKU_ID		"ntk"
#define MAXMSGSZ		8192

/* Pkt's op definitions:
 * The request and replies are in request.h
 */

/*Pkt's flags*/
#define SEND_ACK		1

typedef struct
{
	inet_prefix from;
	inet_prefix to;
	int sk;
	u_short port;
	int flags;
	struct pkt_hdr hdr;
	char *msg;
}PACKET;
	
struct pkt_hdr
{
	char ntk_id[3];
	int id;
	u_char op;
	size_t sz;
};
#define PACKET_SZ(sz) (sizeof(pkt_hdr)+(sz))
		
/*Broadcast packet*/
struct brdcast_hdr
{
	int id;
	u_int g_node;		/*The g_node the brdcast_pkt is restricted to*/
	u_short gttl;		/*Gnode ttl: How many gnodes the packet can traverse*/
	size_t sz;		/*Sizeof(the pkt)*/
	char flags;		/*Various flags*/
}

typedef char * brdcast_pkt;	/*Where bdcast_hdr and the real brdcast packet are*/
#define BRDCAST_SZ(pkt_sz) (sizeof(struct brdcast_hdr)+(pkt_sz))

/*Tracer packet. It is encapsulated in a broadcast pkt*/
struct tracer_hdr
{
	__u16 ipstart;
	int hops;
}

struct tracer_node
{
	__u16 node;
	struct timeval *rtt;
}

typedef char * tracer_pkt;
#define TRACERPKT_SZ(hop) (sizeof(struct tracer_hdr)+sizeof((struct tracer_node)*(hop)))

/*The nodeblock of the node*/
struct node_hdr
{
	struct sockaddr ip;		/*Ip of the node*/
	__u16 links;			/*Number of r_nodes*/
};

struct r_node
{	
	struct sockaddr r_node;         /*Ip of the r_node*/
	struct timeval  rnode_t;	/*node <-> r_node time*/	
};

typedef char * nodeblock;	 /*Where node_hdr and r_nodes are allocated*/
#define NODEBLOCK_SZ(links) (sizeof(struct node_hdr)+sizeof((struct r_node)*(links)))

/*This block is used to send the int_map*/
struct int_map_hdr
{
	u_short root_node;
	size_t int_map_sz;
	size_t rblock_sz;
};
typedef char * int_map_block;
#define INT_MAP_BLOCK_SZ(int_map_sz, rblock_sz) (sizeof(struct int_map_hdr)+(int_map_sz)+(rblock_sz))

/*This block is used to send the int_map*/
struct ext_map_hdr
{
	u_int root_gnode;
	size_t ext_map_sz;
	size_t rblock_sz;
};
typedef char * ext_map_block;
#define EXT_MAP_BLOCK_SZ(ext_map_sz, rblock_sz) (sizeof(struct ext_map_hdr)+(ext_map_sz)+(rblock_sz))

struct set_route_hdr
{
	u_int hops;
};

struct set_route_pkt
{
	char flags;
	struct sockaddr node;
};
typedef char * set_route_block;
#define SET_ROUTE_BLOCK_SZ(hops) (sizeof(struct set_route_hdr)+((sizeof(struct set_route_pkt)*(hops))))


/*Functions' declarations*/
void pkt_addfrom(PACKET *pkt, inet_prefix *from);
void pkt_addto(PACKET *pkt, inet_prefix *to);
void pkt_addsk(PACKET *pkt, int sk);
void pkt_addport(PACKET *pkt, u_short port);
void pkt_addflags(PACKET *pkt, int flags);
void pkt_addhdr(PACKET *pkt, struct pkt_hdr *hdr);
void pkt_addmsg(PACKET *pkt, char *msg);
void pkt_free(PACKET *pkt);
char *pkt_pack(PACKET pkt);
PACKET *pkt_unpack(char *pkt);
int pkt_verify_hdr(PACKET pkt);
ssize_t pkt_send(PACKET pkt);
ssize_t pkt_recv(PACKET *pkt);
pkt_fill_hdr(struct pkt_hdr *hdr, int id, u_char op, size_t sz);
pkt_err(PACKET pkt, int err);
int pkt_exec(PACKET pkt);
