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

int tracer_pkt_start_mutex=0;

/*Functions definition. Damn I hate to use function with a lot of args. It isn't elegant*/
int tracer_verify_pkt(tracer_chunk *tracer, u_int hops, map_node *real_from, u_char level);
char *tracer_pack_pkt(brdcast_hdr *, tracer_hdr *, tracer_chunk *, bnode_hdr *, bnode_chunk *);
int tracer_split_bblock(void *, size_t, bnode_hdr *, bnode_chunk *, size_t *);
int tracer_store_pkt(void *, u_char, tracer_hdr *, tracer_chunk *, void *, size_t, u_short *,  char *, size_t *);
int tracer_unpack_pkt(PACKET, brdcast_hdr *, tracer_hdr *, tracer_chunk *, bnode_hdr *, size_t *);
tracer_chunk *tracer_add_entry(void *, void *, tracer_chunk *, u_int *, u_char);
bnode_hdr *tracer_build_bentry(void *, void *, bnode_chunk *, u_char);

#define TRACER_PKT_EXCLUDE_VARS		map_node *node, map_node *from, int pos,\
					int excl_gid, u_char excl_level
int tracer_pkt_build(u_char, int, int, brdcast_hdr *, tracer_hdr *, tracer_chunk *, u_short, char *, size_t, PACKET *);
int tracer_pkt_send(int(*is_node_excluded)(TRACER_PKT_EXCLUDE_VARS), int gid, u_char level, map_node *from, PACKET pkt);
int exclude_from_and_glevel_and_setreplied(TRACER_PKT_EXCLUDE_VARS);
int exclude_from_and_glevel_and_closed(TRACER_PKT_EXCLUDE_VARS);
int exclude_from_and_glevel(TRACER_PKT_EXCLUDE_VARS);
