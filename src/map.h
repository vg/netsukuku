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
#include "inet.h"
#include "ipv6-gmp.c"

#define MAXGROUPNODE		0x281
#define MAXROUTES	 	20

/*****The real map stuff*****/
/***flags**/
#define MAP_ME		1		/*The root_node, in other words, me ;)*/
#define MAP_HNODE	(1<<1)		/*Hooking node. One node is hooking when it is connecting to netsukuku*/
#define MAP_SNODE	(1<<2)
#define MAP_DNODE	(1<<3)
#define MAP_BNODE	(1<<4)		/*The node is a border_node*/
#define MAP_GNODE	(1<<5)
#define MAP_RNODE	(1<<6)		/*If a node has this set, it is one of the rnodes*/
#define MAP_UPDATE	(1<<7)		/*If it is set, the corresponding route in the krnl will be updated*/
#define MAP_VOID	(1<<8)		/*It indicates a non existent node*/

/*map_rnode is what map_node.r_node points to*/
typedef struct
{
/*Questa flag non serve perche' c'e' gia' quella in r_node->flags	
 * u_char 		flags; */
	u_int	 	*r_node;		 /*It's the pointer to the struct of the r_node in the map*/
	struct timeval  rtt;	 		 /*node <-> r_node round trip time*/
	
	struct timeval  trtt;			/*node <-> root_node total rtt: The rtt to reach the root_node 
	 					  starting from the node which uses this rnode. 
	 * Cuz I've explained it in such a bad way I make an example:
	 * map_node node_A; From node_A "node_A.links"th routes to the root_node start. 
	 * So I have "node_A.links"th node_A.r_node[s], each of them is a different route to reach the root_node. 
	 * With the node_A.r_node[route_number_to_follow].trtt I can get the rtt needed to reach the root_node 
	 * starting from the node_A using the route_number_to_follow. Gotcha? I hope so.
	 * Note: The trtt is mainly used for the sort of the routes*/
}map_rnode;

typedef struct
{
	u_short 	flags;
	u_int		brdcast;	 /*Pkt_id of the last brdcast_pkt sent by this node*/
	__u16		links;		 /*Number of r_nodes*/
	map_rnode	*r_node;	 /*This structs will be kept in ascending order considering their rnode_t.rtt*/
}map_node;

/* This is the internal map and it will be MAXGROUPNODE big.
 * The position in the map of each struct corresponds to its relative ip. For
 * example, if the map goes from 192.128.1.0 to 192.128.3.0, the map will have 512
 * structs, the first one will correspond to 192.168.1.0, the 50th to 192.168.1.50 and
 * so on.
 */
typedef map_node * int_map;    				      


/*****QSPN int_map (It's identical to the normal int_map)****
 * Anyway there are a few differences in the qmap:
 * - map_node.links is the number of routes
 * - map_node.r_node points to its r_node that is part of the route to the root_node.
 *   The only execption is the root_node itself. The root_node's map_node.r_node keeps
 *   all its rnodes as a normal (non qspn) map would.
 */
typedef map_node qmap_node;
typedef qmap_node *qint_map;


/***Groupnode stuff***/
#define GMAP_ME		1
#define GMAP_VOID	(1<<1)
#define GMAP_BNODE	(1<<2)
#define GMAP_FULL	(1<<3)		/*The gnode is full!! aaahh, run away!*/

/*Given an ip number it returns the corresponding groupnode id*/
#define IP2GNODE(ip) ((ip)/MAXGROUPNODE) /*TODO:
					   USARE le ipv6-gmp per dividere!*/

/*Converts an address of a struct in the map to a Group_node Id*/
#define GMAP2GI(mapstart, mapoff)	((((mapoff)-(mapstart))/sizeof(map_gnode)))
/*Converts a Groupnode Id to an address of a struct in the map*/
#define GI2GMAP(mapstart, gi)		(((gi)*sizeof(map_gnode))+(mapstart))

typedef struct
{
        /*The groupnode will cover the range from me.ipstart to me.ipstart+MAXGROUPNODE*/

	__u16 		gid;		/*Gnode Id*/
	__u16		layer;
	__u16 		seeds;		/*The number of active static nodes connected to this gnode*/

	/*Th4 g_m4p starts here. Note that it is a normal map: each node-struct has the pointer
	 * to the nodes connected to it*/
	u_short 	flags;
	__u16		links;		 /*Number of gnode connected to this gnode*/
	map_rnode	*r_node;	 /*This structs will be kept in ascending order considering their rnode_t.rtt*/

	/*In the own g_node entry there are also the boarder_nodes in g_node.r_node*/
}map_gnode;

typedef map_gnode * ext_map;

#define INTMAP_END(mapstart)	((sizeof(map_node)*MAXGROUPNODE)+(mapstart))
/*Converts an address of a struct in the map to an ip*/
void maptoip(u_int mapstart, u_int mapoff, inet_prefix ipstart, inet_prefix *ret)
{
	if(ipstart.family==AF_INET) {
		ret->data[0]=((mapoff-mapstart)/sizeof(map_node))+ipstart.data[0];
		ret->family=AF_INET;
		ret->len=4;
	} else {
		ret->family=AF_INET6;
		ret->len=16;
		memcpy(ret->data, ipstart.data, sizeof(inet_prefix));
		sum_int(((mapoff-mapstart)/sizeof(map_node)), ret->data);
	}
}

/*Converts an ip to an address of a struct in the map*/
int iptomap(u_int mapstart, inet_prefix ip, inet_prefix ipstart, u_int *ret)
{
	if(ip.family==AF_INET)
		*ret=((ip.data[0]-ipstart.data[0])*sizeof(map_node))+mapstart;
	else {
		__u32 h_ip[4], h_ipstart[4];

		memcpy(h_ip, ip.data, 4);
		memcpy(h_ipstart, ipstart.data, 4);

		sub_128(h_ip, h_ipstart);
		/*The result is always < MAXGROUPNODE, so we can take for grant that
		 * we have only one u_int
		 */
		ret=h_ipstart[0]*sizeof(map_node)+mapstart;
		/*TODO: bisogna usare h_ipstart[0] o h_ipstart[3]?? Spero che sia 0 perche' e' in ntohl*/
	}
	if(*ret > INTMAP_END(mapstart)) {
		/*Ok, this is an extern ip to our gnode. We return the gnode_id of this ip*/
		ret=IP2GNODE(*ret);
		return 1;
	}
	return 0;
}
	
/*TODO: spostare da un'altra parte!*/
#define MILLISEC(x)	(((x).tv_sec*1000)+((x).tv_usec/1000))


/* * * Functions' declaration * * */

map_node *init_map(size_t len);
void free_map(map_node *map, size_t count);
map_rnode *rnode_insert(map_rnode *buf, size_t pos, map_rnode *new);
map_rnode *map_rnode_insert(map_node *node, size_t pos, map_rnode *new);
map_rnode *rnode_add(map_node *node, map_rnode *new);
void rnode_swap(map_rnode *one, map_rnode *two);
void rnode_del(map_node *node, size_t pos);
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
int mod_rnode_addr(map_node *node, map_node *map_start, map_node *new_start);
map_rnode *get_rnode_block(map_node *map, int *count);
int store_rnode_block(map_node *map, map_rnode *rblock, int count);
