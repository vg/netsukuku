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
#include <sys/time.h>
#include <sys/types.h>
#include <gmp.h>
#include "map.h"

/* * * Groupnode stuff * * */
#define GMAP_ME		1
#define GMAP_VOID	(1<<1)
#define GMAP_BNODE	(1<<2)
#define GMAP_FULL	(1<<3)		/*The gnode is full!! aaahh, run away!*/

/*Converts an address of a struct in the map to a Group_node Id*/
#define GMAP2GI(mapstart, mapoff)	((((mapoff)-(mapstart))/sizeof(map_gnode)))
/*Converts a Groupnode Id to an address of a struct in the map*/
#define GI2GMAP(mapstart, gi)		(((gi)*sizeof(map_gnode))+(mapstart))

typedef struct
{
        /*The groupnode will cover the range from me.ipstart to me.ipstart+MAXGROUPNODE*/

	__u16 		gid;		/*Gnode Id*/
	/*__u16		layer; 		  not used anymore*/
	__u16 		seeds;		/*The number of active static nodes connected to this gnode.
					  If seeds == MAXGROUPNODE, the gnode is full ^_^*/

	/*Th4 g_m4p starts here. Note that it is a normal map: each node-struct has the pointer
	 * to the nodes connected to it*/
	map_node	g;

	/*In the own g_node entry there are also the boarder_nodes in g_node.r_node*/
}map_gnode;


/* * * Quadro Group stuff * * */
#define IPV4_LEVELS	3		/* 0 <= level <IPV4_LEVELS */
#define IPV6_LEVELS	13		/* 0 <= level <IPV6_LEVELS */
#define MAX_LEVELS	IPV6_LEVELS
#define GET_LEVELS(family)	({ (family) == AF_INET ? IPV4_LEVELS : IPV6_LEVELS; })

/* Struct used to keep all the quadro_group ids of a node. (The node is part of this
 * quadro groups)*/
typedef struct {
	u_char      levels;		 /*How many levels of quadro_group we have*/
	int         gid[MAX_LEVELS];	 /*Group ids. Each element is the gid of the quadrogroup in the 
					   relative level. (ex: gid[n] is the gid of the quadropgroup a 
					   the n level)*/
	map_gnode  *gnode[MAX_LEVELS];	 /*Each element is a pointer to the relative gnode in the ext_map*/
	inet_prefix ipstart[MAX_LEVELS]; /*The ipstart of each group*/
}quadro_group;

/* MAXGROUPNODE per levels. In each levels there is a number 
 * equal to the max number of nodes present in that level.
 * So maxgroupnode_levels[level]=MAXGROUPNODE^level;
 * It is initialized at the start, so the work is done only once*/
mpz_t maxgroupnode_levels[MAX_LEVELS];

/***This block is used to send the ext_map*/
struct ext_map_hdr
{
	quadro_group quadg;
	size_t ext_map_sz; 		/*It's the sum of all the gmaps_sz*/
	size_t gmap_sz[MAX_LEVELS];	/*The size of each gmap*/
	size_t total_rblock_sz;		/*The sum of all rblock_sz*/
	size_t rblock_sz[MAX_LEVELS];	/*The size of the rblock of each gmap*/
};
/*The ext_map_block is:
 * 	struct ext_map_hdr hdr;
 * 	char ext_map[ext_map_sz];
 * 	char rnode_blocks[total_rblock_sz];*/
#define EXT_MAP_BLOCK_SZ(ext_map_sz, rblock_sz) (sizeof(struct ext_map_hdr)+(ext_map_sz)+(rblock_sz))


/* * * Functions' declaration * * */
void maxgroupnode_level_init(void);
void maxgroupnode_level_free(void);

u_short iptogid(inet_prefix ip, u_char level);
void gidtoipstart(u_short gid, u_char level, int family, inet_prefix *ip);
void iptoquadg(inet_prefix ip, map_gnode *mapstart, quadro_group *qg);

map_gnode *init_gmap(u_short groups);
void free_gmap(map_gnode *gmap, u_short groups);
map_gnode *init_extmap(u_char levels, u_short groups);
void free_extmap(map_gnode **ext_map, u_char levels, u_short groups);

map_rnode *gmap_get_rblock(map_gnode *map, int maxgroupnode, int *count);
int gmap_store_rblock(map_gnode *map, int maxgroupnode, map_rnode *rblock);
map_rnode *extmap_get_rblock(map_gnode **ext_map, u_char levels, int maxgroupnodes, int *ret_count);
int extmap_store_rblock(map_gnode **ext_map, u_char levels, int maxgroupnode, map_rnode **rblock);

int verify_ext_map_hdr(struct ext_map_hdr *emap_hdr);
void free_extmap_rblock(map_rnode **rblock, u_char levels);
char *pack_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, size_t *pack_sz);
map_gnode *unpack_extmap(char *package, size_t pack_sz, quadro_group *quadg);
int save_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, char *file);
map_gnode *load_extmap(char *file, quadro_group *quadg);
