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

#undef  QSPN_EMPIRIC

#ifndef QSPN_EMPIRIC
	#define MAXGROUPNODE		641
	#define MAXROUTES	 	20
#else
	#define MAXGROUPNODE		20
	#define MAXROUTES	 	5
#endif /*QSPN_EMPIRIC*/

#define MAXLINKS		MAXROUTES
#define MAXRTT			10		/*Max node <--> node rtt (in sec)*/

/*****The real map stuff*****/
/***flags**/
#define MAP_ME		1		/*The root_node, in other words, me ;)*/
#define MAP_VOID	(1<<1)		/*It indicates a non existent node*/
#define MAP_HNODE	(1<<2)		/*Hooking node. One node is hooking when it is connecting to netsukuku*/
#define MAP_SNODE	(1<<3)		/*Static Node*/
#define MAP_BNODE	(1<<4)		/*The node is a border_node*/
#define MAP_ERNODE	(1<<5)		/*It is an External Rnode*/
#define MAP_GNODE	(1<<6)		/*It is a gnode*/
#define MAP_RNODE	(1<<7)		/*If a node has this set, it is one of the rnodes*/
#define MAP_UPDATE	(1<<8)		/*If it is set, the corresponding route in the krnl will be updated*/
#define QSPN_CLOSED	(1<<9)		/*This flag is set only to the rnodes, it puts a link in a QSPN_CLOSED state*/
#define QSPN_OPENED	(1<<10)		/*It puts a link in a QSPN_OPEN state*/
#define QSPN_REPLIED	(1<<11)		/*When the node send the qspn_reply it will never reply again to the same qspn*/
#define QSPN_BACKPRO	(1<<12)		/*This marks the r_node where the QSPN_BACKPRO has been sent*/
#define QSPN_OLD	(1<<13)		/*If a node isn't updated by the current qspn_round it is marked with QSPN_ROUND.
					  If in the next qspn_round the same node isn't updated it is removed from the map.*/
#ifdef QSPN_EMPIRIC
#define QSPN_STARTER	(1<<14)		/*Used only by qspn-empiric.c*/
#endif
/* 			    *** Map notes ***
 * The map is a block of MAXGROUPNODE map_node struct. It is a generic map and it is
 * used to keep the qspn_map, the internal map and the external map.
 * The position in the map of each struct corresponds to its relative ip. For
 * example, if the map goes from 192.128.1.0 to 192.128.3.0, the map will have 512
 * structs, the first one will correspond to 192.168.1.0, the 50th to 192.168.1.50 and
 * so on.
 */
/*map_rnode is what map_node.r_node points to*/
typedef struct
{
#ifdef QSPN_EMPIRIC
	u_short		flags;
#endif
	u_int	 	*r_node;		 /*It's the pointer to the struct of the r_node in the map*/
	struct timeval  rtt;	 		 /*node <-> r_node round trip time*/
	
	struct timeval  trtt;			 /*node <-> root_node total rtt: The rtt to reach the root_node 
	 					  starting from the node which uses this rnode. 
	 * Cuz I've explained it in such a bad way I make an example:
	 * map_node node_A; From node_A "node_A.links"th routes to the root_node start. 
	 * So I have "node_A.links"th node_A.r_node[s], each of them is a different route to reach the root_node. 
	 * With the node_A.r_node[route_number_to_follow].trtt I can get the rtt needed to reach the root_node 
	 * starting from the node_A using the route_number_to_follow. Gotcha? I hope so.
	 * Note: The trtt is mainly used to sort the routes*/
}map_rnode;

typedef struct
{
	u_short 	flags;
#ifdef QSPN_EMPIRIC
	u_int		brdcast[MAXGROUPNODE];
#else
	u_int		brdcast;	 /*Pkt_id of the last brdcast_pkt sent by this node*/
#endif /*QSPN_EMPIRIC*/
	__u16		links;		 /*Number of r_nodes*/
	map_rnode	*r_node;	 /*This structs will be kept in ascending order considering their rnode_t.rtt*/
}map_node;

#define MAXRNODEBLOCK		MAXLINKS*MAXGROUPNODE*sizeof(map_rnode)
#define INTMAP_END(mapstart)	((sizeof(map_node)*MAXGROUPNODE)+(mapstart))

/*This block is used to send/save the int_map and the bnode_map*/
struct int_map_hdr
{
	u_short root_node;
	size_t int_map_sz;
	size_t rblock_sz;
};

/*The int_map_block is:
 * 	struct int_map_hdr hdr;
 * 	char map_node[int_map_sz];
 * 	char map_rnode[rblock_sz];
 */
#define INT_MAP_BLOCK_SZ(int_map_sz, rblock_sz) (sizeof(struct int_map_hdr)+(int_map_sz)+(rblock_sz))

/* * * Functions' declaration * * */
/*conversion functions*/
int pos_from_node(map_node *node, map_node *map);
map_node *node_from_pos(int pos, map_node *map);
void maptoip(u_int mapstart, u_int mapoff, inet_prefix ipstart, inet_prefix *ret);
int iptomap(u_int mapstart, inet_prefix ip, inet_prefix ipstart, u_int *ret);

map_node *init_map(size_t len);
void free_map(map_node *map, size_t count);
void map_node_del(map_node *node);

map_rnode *rnode_insert(map_rnode *buf, size_t pos, map_rnode *new);
map_rnode *map_rnode_insert(map_node *node, size_t pos, map_rnode *new);
map_rnode *rnode_add(map_node *node, map_rnode *new);
void rnode_swap(map_rnode *one, map_rnode *two);
void rnode_del(map_node *node, size_t pos);
void rnode_destroy(map_node *node);
int rnode_find(map_node *node, map_node *n);

int rnode_rtt_compar(const void *a, const void *b);
void rnode_rtt_order(map_node *node);
int rnode_trtt_compar(const void *a, const void *b);
void rnode_trtt_order(map_node *node);
void map_routes_order(map_node *map);

int get_route_rtt(map_node *node, u_short route, struct timeval *rtt);
void rnode_set_trtt(map_node *node);
void rnode_recurse_trtt(map_rnode *rnode, int route, struct timeval *trtt);
void node_recurse_trtt(map_node *node);
void map_set_trtt(map_node *map);
map_node *get_gw_node(map_node *node, u_short route);

int merge_maps(map_node *base, map_node *new, map_node *base_root, map_node *new_root);
int mod_rnode_addr(map_rnode *node, int *map_start, int *new_start);
int get_rnode_block(int *map, map_node *node, map_rnode *rblock, int rstart);
map_rnode *map_get_rblock(map_node *map, int *addr_map, int maxgroupnode, int *count);
int store_rnode_block(int *map, map_node *node, map_rnode *rblock, int rstart);
int map_store_rblock(map_node *map, int *addr_map int maxgroupnode, map_rnode *rblock);

int verify_int_map_hdr(struct int_map_hdr *imap_hdr, int maxgroupnode, int maxrnodeblock);
char *pack_map(map_node *map, int *addr_map, int maxgroupnode, map_node *root_node, size_t *pack_sz);
map_node *unpack_map(char *pack, size_t pack_sz, int *addr_map, map_node *new_root, int maxgroupnode, int maxrnodeblock);
int save_map(map_node *map, map_node *root_node, char *file);
map_node *load_map(char *file, map_node **new_root);
