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

/* Used for: ip.data[0]=HOOKING_IP; */
#define HOOKING_IP  0x80000001      /* 128.0.0.1  (in host long) */
#define HOOKING_IP6 0x80000001

/* 
 * The free_nodes pkt is used to send the list of all the free/not used
 * nodes/gnodes present at level fn_hdr.level. 
 * If level is == 0 then the pkt contains the list of free nodes of the fn_hdr.gid
 * gnode. 
 * If level is > 0 then it contains the list of free gnodes which are inside
 * the gnode with `fn_hdr.gid' gid of the `fn_hdr.level'th level. So the free gnodes
 * are part of the (fn_hdr.level - 1)th level.
 * 
 * The free nodes pkt contains also the the qspn round time of all levels.
 */
struct free_nodes_hdr
{
	u_char 		max_levels;	/* How many levels we are managing */

	inet_prefix 	ipstart;	/* The ipstart of the gnode */
	u_char 		level;		/* The level where the gnode belongs */
	u_short  	gid;		/* The gnode id */
	u_short		nodes;		/* The number of free nodes/gnodes. 
					   It cannot be greater than MAXGROUPNODE */
};
#define FREE_NODES_SZ(levels, nodes) (sizeof(struct free_nodes_hdr) +	      \
				 	((levels) * sizeof(struct timeval)) + \
					  (sizeof(u_short) * (nodes)))

/* 
 * the free_nodes block is:
 *	struct timeval  qtime[fn_hdr.max_levels];  qspn round time: how many seconds passed away
 *						   since the previous qspn round. There's a qtime
 *						   for each fn_hdr.max_levels level
 *	u_short		free_nodes[fn_hdr.nodes];  If free_nodes[x] is the position of the node in the
 *						   map.
 * The free_nodes pkt is:
 *	fn_hdr;
 *	fn_block;
 */

int get_free_nodes(inet_prefix to, struct timeval to_rtt, struct free_nodes_hdr *fn_hdr, u_short *nodes, struct timeval *qtime);
int put_free_nodes(PACKET rq_pkt);

int put_ext_map(PACKET rq_pkt);
map_gnode **get_ext_map(inet_prefix to, quadro_group *new_quadg);

int put_int_map(PACKET rq_pkt);
map_node *get_int_map(inet_prefix to, map_node **new_root);

int put_bnode_map(PACKET rq_pkt);
map_bnode **get_bnode_map(inet_prefix to, u_int **bmap_nodes);

int create_gnodes(inet_prefix *ip, int final_level);
void set_ip_and_def_gw(char *dev, inet_prefix ip);

int hook_init(void);
int netsukuku_hook(void);

/*int snode_transfrom();*/
