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
#define GMAP_ME		MAP_ME		/*1*/
#define GMAP_VOID	MAP_VOID	/*(1<<1)*/
#define GMAP_FULL	(1<<2)		/*The gnode is full!! aaahh, run away!*/

/* This is the holy external_map. Each struct corresponds to a groupnode. This groupnode
 * cointains MAXGROUPNODE nodes if we are at level 1 or MAXGROUPNODE groups.
 * The map is equal to the int_map, infact, a map_node is embedded in a map_gnode. This
 * int_map uses the QSPN_MAP_STYLEII (see qspn.h). */
typedef struct
{
	/*The gnode_map starts here. Note that it is a normal map. (See map.h). It is here,
	 * at the top of the struct to allow to manipulate a map_gnode as a map_node with
	 * the help of the magic cast. The cast is heavily used in qspn.c*/
	map_node	g;
	
	u_char 		flags;
	__u16 		seeds;		/*The number of active static nodes connected to this gnode.
					  If seeds == MAXGROUPNODE, the gnode is full ^_^*/
}map_gnode;


/* * * Levels stuff * * *
 * These are the levels of the external_map. Note that the 0 level is never used 
 * for the ext_map because it corresponds to the internal map. Btw the 0 level is 
 * counted so the number of LEVELS includes it too. 
 * But we have to add another extra level: the last exiled level. It is also never 
 * used but it is vital, cause, its gnode 0 includes the entire Netsukuku, the other
 * gnodes aren't used, it is a mere symbol. We call it the unity level.
 *
 * All the structs/arrays related to the external map, and the ext_map itself, don't
 * use the EXTRA_LEVELS, thus, they lack of the zero level. To retrieve the position 
 * in the array from the level the _EL macro must be used. 
 * These arrays/structs are: quadg.gnode, rblock, ext_map.*/
#define EXTRA_LEVELS	2		/*One is the Zero Level, and the other one is the Final Level*/
#define IPV4_LEVELS	(3+EXTRA_LEVELS)
#define IPV6_LEVELS	(13+EXTRA_LEVELS)
#define MAX_LEVELS	IPV6_LEVELS
#define GET_LEVELS(family)	({ (family) == AF_INET ? IPV4_LEVELS : IPV6_LEVELS; })

/* To use the right level. (Ext_mapLevel)*/
#define _EL(level)    ((level)-1)
/* And to restore it. (NormalLevel)*/
#define _NL(level)    ((level)+1)

/* Struct used to keep all the quadro_group ids of a node. (The node is part of this
 * quadro groups)*/
typedef struct {
	u_char      levels;		 /*How many levels we have*/
	int         gid[MAX_LEVELS];	 /*Group ids. Each element is the gid of the quadrogroup in the 
					   relative level. (ex: gid[n] is the gid of the quadropgroup a 
					   the n level)*/
	map_gnode  *gnode[MAX_LEVELS-EXTRA_LEVELS]; /*Each element is a pointer to the relative gnode in the 
						      ext_map. It has levels-EXTRA_LEVELS elements.*/
	inet_prefix ipstart[MAX_LEVELS]; /*The ipstart of each quadg.gid in their respective levels*/
}quadro_group;

/*These are the flags passed to iptoquadg()*/
#define QUADG_IPSTART 1
#define QUADG_GID     (1<<1)
#define QUADG_GNODE   (1<<2)

/* Each array element of maxgroupnode_levels is:
 * maxgroupnode_levels[x]=MAXGROUPNODE^x;
 * This is used to get the max number of nodes per levels, in fact,
 * this number is equal to MAXGROUPNODE^(level+1);
 * Maxgroupnode_levels is initialized at the start, so the work is done only once*/
mpz_t maxgroupnode_levels[MAX_LEVELS+1];

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

/* The root_node at level 0 who has a rnode, who isn't of the same level, uses this external_rnode_struct
 * to refer to that particular rnode.*/
typedef struct {
	map_node	node;
	inet_prefix	ip;
	quadro_group 	quadg;
}ext_rnode;

/*This cache keeps the list of all the ext_rnode used.*/
struct ext_rnode_cache {
	struct ext_rnode_cache *next;
	struct ext_rnode_cache *prev;

	ext_rnode	*e;		/*The pointer to the ext_rnode struct*/
	int		rnode_pos;	/*The ext_rnode position in the root_node's rnodes*/
};
typedef struct ext_rnode_cache ext_rnode_cache;

/* * * Functions' declaration * * */
int pos_from_gnode(map_gnode *gnode, map_gnode *map);
map_node *gnode_from_pos(int pos, map_gnode *map);

void maxgroupnode_level_init(void);
void maxgroupnode_level_free(void);

u_short iptogid(inet_prefix ip, u_char level);
void gidtoipstart(u_short *gid, u_char total_levels, u_char levels, int family,
		  inet_prefix *ip);
void iptoquadg(inet_prefix ip, map_gnode **ext_map, quadro_group *qg, char flags)
void quadg_free(quadro_group *qg);
void quadg_destroy(quadro_group *qg);
void random_ip(inet_prefix *ipstart, int final_level, int final_gid, 
		int total_levels, map_gnode **ext_map, int only_free_gnode, 
		inet_prefix *new_ip);
int quadg_diff_gids(quadro_group qg_a, quadro_group qg_b);
int e_rnode_find(ext_rnode_cache *erc, quadro_group *qg);

map_gnode *init_gmap(u_short groups);
void reset_gmap(map_gnode *gmap, u_short groups);
map_gnode **init_extmap(u_char levels, u_short groups);
void free_extmap(map_gnode **ext_map, u_char levels, u_short groups);

int  g_rnode_find(map_gnode *gnode, map_gnode *n);
int  extmap_find_level(map_gnode **ext_map, map_gnode *gnode, u_char max_level);
void gmap_node_del(map_gnode *gnode);

map_rnode *gmap_get_rblock(map_gnode *map, int maxgroupnode, int *count);
int gmap_store_rblock(map_gnode *map, int maxgroupnode, map_rnode *rblock);
map_rnode *extmap_get_rblock(map_gnode **ext_map, u_char levels, int maxgroupnodes, int *ret_count);
int extmap_store_rblock(map_gnode **ext_map, u_char levels, int maxgroupnode, map_rnode **rblock);

int verify_ext_map_hdr(struct ext_map_hdr *emap_hdr);
void free_extmap_rblock(map_rnode **rblock, u_char levels);
char *pack_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, size_t *pack_sz);
map_gnode *unpack_extmap(char *package, size_t pack_sz, quadro_group *quadg);
int save_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, char *file);
map_gnode **load_extmap(char *file, quadro_group *quadg);
