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

#include "map.h"

/***Tracer packet. It is encapsulated in a broadcast pkt*/
typedef struct
{
	u_int hops;
	u_int bblocks;		/*How many bnode blocks are incapsulated in the pkt (if any)*/
}tracer_hdr;

typedef struct
{
	__u16 node;
	struct timeval *rtt;
}tracer_chunk;
#define TRACERPKT_SZ(hop) (sizeof(tracer_hdr)+sizeof((tracer_chunk)*(hop)))


/* boarder node block: this is the block which keeps the gnodes linked to the `bnode' boarder_node. 
 * When a bnode has to add his entry in the tracer_pkt it encapsulates the bnode_block at the end
 * of the packet, in this way it is possible to know all the gnodes linked to the bnode's gnode.
 * Note: It is possible that the packet passes trough many bnodes, in this case the bnode block
 * is always put at the end, ex: 
 * |pkt_hdr|brdcast_hdr|tracer_hdr|tracer_chunks|bnode_hdr|bnode_chunks|bnode_hdr|bnode_chunks|...
 * and so on.
 */
typedef struct
{
	__u16 bnode;		/*The bnode this bnode_block belongs to*/
	__u16 links;		/*The number of linked gnode*/
}bnode_hdr;
typedef struct
{
	__u16 gnode;
	struct timeval *rtt;
}bnode_chunk;
#define BNODEBLOCK_SZ(links) (sizeof(bnode_hdr)+sizeof(bnode_chunk)*(links))

/*These defines make the life easier, son instead of writing int_map_hdr I write bnode_map_hdr.
 * Cool eh? ^_^. int_map_hdr is in pkts.h*/
#define bnode_map_hdr 		int_map_hdr
#define bnode_map_sz   		int_map_sz
#define BNODE_MAP_BLOCK_SZ 	INT_MAP_BLOCK_SZ

int tracer_pkt_start_mutex=0;

/*Functions definition. Damn I hate to use function with a lot of args. It isn't elegant*/
int tracer_verify_pkt(tracer_chunk *, int);
char *tracer_pack_pkt(brdcast_hdr *, tracer_hdr *, tracer_chunk *, bnode_hdr *, bnode_chunk *);
int tracer_split_bblock(void *, size_t, bnode_hdr *, bnode_chunk *, size_t *);
int tracer_store_pkt(map_node *, tracer_hdr *, tracer_chunk *, void *, size_t, int *, char *, size_t *);
int tracer_unpack_pkt(PACKET, brdcast_hdr *, tracer_hdr *, tracer_chunk *, bnode_hdr *, size_t *);
tracer_chunk *tracer_add_entry(map_node *, map_node *, tracer_chunk *, int *);
bnode_hdr *tracer_build_bentry(map_node *, map_node *, bnode_chunk *); 

int tracer_pkt_build(u_char, int, int, brdcast_hdr *, tracer_hdr *, tracer_chunk *, u_short, char *, size_t, PACKET *);
int tracer_pkt_send(int(*is_node_excluded)(map_node *, int), PACKET);
int exclude_from_and_gnode_and_setreplied(map_node *node, map_node *from, int pos);
int exclude_from_and_gnode_and_closed(map_node *node, map_node *from, int pos);
int exclude_from_and_gnode(map_node *node, map_node *from, int pos);
