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
struct tracer_hdr
{
	u_int hops;
	u_int bblocks;		/*How many bnode blocks are incapsulated in the pkt (if any)*/
};
struct tracer_chunk
{
	__u16 node;
	struct timeval *rtt;
};
#define TRACERPKT_SZ(hop) (sizeof(struct tracer_hdr)+sizeof((struct tracer_chunk)*(hop)))
/* boarder node block: this is the block which keeps the gnodes linked to the `bnode' boarder_node. 
 * When a bnode has to add his entry in the tracer_pkt it encapsulates the bnode_block at the end
 * of the packet, in this way it is possible to know all the gnodes linked to the bnode's gnode.
 * Note: It is possible that the packet passes trough many bnodes, in this case the bnode block
 * is always put at the end, ex: 
 * |pkt_hdr|brdcast_hdr|tracer_hdr|tracer_chunks|bnode_hdr|bnode_chunks|bnode_hdr|bnode_chunks|...
 * and so on.
 */
struct bnode_hdr
{
	__u16 bnode;		/*The bnode this bnode_block belongs to*/
	__u16 links;		/*The number of linked gnode*/
};
struct bnode_chunk
{
	__u16 gnode;
	struct timeval *rtt;
};
#define BNODEBLOCK_SZ(links) (sizeof(bnode_hdr)+sizeof(bnode_chunk)*(links))


int tracer_verify_pkt(struct tracer_chunk *tracer, int hops);
char *tracer_pack_pkt(struct bcast_hdr *bcast_hdr, struct tracer_hdr *tracer_hdr, struct tracer_chunk *tracer, 
		      struct bnode_hdr *bhdr, struct bnode_chunk *bchunk);
int tracer_split_bblock(void *bnode_block_start, size_t bblock_sz, struct bnode_hdr *bbl_hdr, struct bnode_chunk *bbl);
int tracer_store_pkt(map_node *map, struct tracer_hdr *tracer_hdr, struct tracer_chunk *tracer, 
		     u_short hops, void *bnode_block_start, size_t bblock_sz);
struct tracer_chunk *tracer_add_entry(map_node *map, map_node *node, struct tracer_chunk *tracer, int *hops);
struct bnode_hdr *tracer_build_bentry(map_node *map, map_node *node, struct bnode_chunk *bnode_chunk); 
