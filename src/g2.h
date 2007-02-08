/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
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

#ifndef GMAP_H
#define GMAP_H

#include "llist.c"
#include "map.h"

/*
 * Gnode ID
 *
 * A number in [0, 255]
 */
typedef nid_t	gid_t;


/*
 * map_gnode.flags
 */
#define GMAP_ME		MAP_ME		/* 1 */
#define GMAP_VOID	MAP_VOID	/* (1<<1) */
#define GMAP_HGNODE	(1<<2)		/* Hooked Gnode. We already hooked at 
					   this gnode */
#define GMAP_FULL	(1<<3)		/* The gnode is full!! aaahh, run
					   away! */

/*
 * map_gnode
 * ---------
 *
 * Structure of a group node (gnode).
 * Each level of the external map is composed by MAXGROUPNODE map_gnode
 * structs.
 *
 * A map_gnode differs lightly from a {-map_node-}.
 * The few informations added are the gnode flags and the `seeds', `gcount'
 * numbers.
 */
typedef struct
{
	/*
	 * We can threat a map_gnode as an extension of the map_node struct,
	 * by placing it at the top. For example:
	 * 	map_gnode *gn;
	 * 	map_node *n;
	 * 	n = (map_node *)gn;
	 * 	n->links;   		// This is a valid statement
	 * 	gn->g.links;		// The same.
	 */
	map_node	g;
	
	u_char 		flags;

	/*
	 * The number of active (g)nodes forming this self map_gnode, minus
	 * one (we don't count ourself).
	 * If seeds == MAXGROUPNODE-1, the gnode is full.
	 */
	uint8_t		seeds;

	/*
	 * The total number of nodes, which are inside this self map_gnode.
	 * See {-TODO_gcount-}
	 */
	u_int		gcount;	
} map_gnode;

/* 
 * MAP_GNODE_PACK_SZ
 * -----------------
 *
 * Returns the size of the {-mapgnode_pack-}.
 * `g_mmr' is the {-MAX_METRIC_ROUTES-} value associated to the map.
 */
#define MAP_GNODE_PACK_SZ(g_mmr)					\
	( MAP_NODE_PACK_SZ(g_mmr) +					\
	  	sizeof(u_char) + sizeof(uint8_t) + sizeof(int) )


/*
 * |{ZERO LEVEL}|
 *
 * The level 0, formed by single nodes, is called "zero level".
 * Its corresponding map is the {-internal map-}. For this reason,
 * the {-external map-} doesn't include it and as a consequence, if the total
 * number of levels is L, then the external map has only L-1 levels.
 *
 * See also {-EL_macro-}, {-extmap-}.
 */
#define ZERO_LEVEL	1

/* 
 * |{EL_macro}|
 *
 * All the arrays of levels [1] related to the external map, and the ext_map 
 * itself, don't use the EXTRA_LEVELS. For this reason, they lack of
 * the {-ZERO LEVEL-}.
 * This means that the level 1 is in the position 0 of the array, 
 * level 2 in 1, and so on.
 * To simplify the access to the array use the {-_EL-} macro:
 * 	ext_map[_EL(1)]  <--  access level 1 of the external map
 * which is the same of:
 * 	ext_map[0]
 *
 * The _NL() macro does the opposite of _EL().
 *
 * These macros are very stupid, but useful to avoid to worry too much about
 * using the right array index.
 *
 * [1] Some of these arrays of levels are: 
 *     nnet.gnode, rblock, ext_map, qspn_gnode_count.
 */
#define _EL(level)    ((level)-ZERO_LEVEL)
#define _NL(level)    ((level)+ZERO_LEVEL)

/*
 * Total number of levels using the ipv4
 * 
 * Note: the {-ZERO LEVEL-} is included in the count.
 */
#define IPV4_LEVELS		(3+ZERO_LEVEL)

/*
 * Total number of levels using the ipv6
 *
 * Note: the {-ZERO LEVEL-} is included in the count.
 */
#define IPV6_LEVELS		(15+ZERO_LEVEL)

#define MAX_LEVELS		IPV6_LEVELS

/*
 * GET_LEVELS
 * ----------
 *
 * Returns the maximum number of levels of a network built using `family',
 * i.e. AF_INET or AF_INET6.
 */
#ifdef DEBUG
#define GET_LEVELS(family)						\
({ 									\
	if((family) != AF_INET && (family) != AF_INET6)			\
		fatal("GET_LEVELS: family not specified!");		\
	(family) == AF_INET ? IPV4_LEVELS : IPV6_LEVELS;		\
 })
#else
#define GET_LEVELS(family) ({ (family)==AF_INET ? IPV4_LEVELS : IPV6_LEVELS; })
#endif

/*
 * Calls GET_LEVELS using the global variable {-my_family-}, which contains the
 * currently used family. The returned value is either {-IPV4_LEVELS-} or
 * {-IPV6_LEVELS-}.
 */
#define FAMILY_LVLS		(GET_LEVELS(my_family))

/* 
 * NODES_PER_LEVEL
 * ---------------
 *
 * Returns the maximum number of nodes which can reside in
 * a gnode of the `lvl'th level.
 *
 * WARNING: if `lvl' == 4 you need to subtract at least 1, in this way:
 * 		NODES_PER_LEVEL(4)-1
 * 	    if `lvl' > 4, overflow problems may occur, see {-TODO_gcount-}.
 */
#define NODES_PER_LEVEL(lvl)	((1<<(MAXGROUPNODE_BITS*(lvl))))

/*
 * ipstart_t
 * ---------
 *
 * The ipstart is the full IP of a node or gnode.
 *
 * Generally the ipstart is associated to a level.
 * The ipstart of level 0 is the full IP of a single node, f.e. 11.22.33.44.
 * The ipstart of level 1 is the CIDR [1] IP of a gnode of level 1, f.e.
 * 11.22.33.0/24
 * So the same ipstart of level 2 is 11.22.0.0/16
 *
 * [1] See {-CITE_CIDR-}
 */
typedef inet_prefix ipstart_t;

/* 
 * Node network						|{nodenet}|
 * ============
 * 
 * The nodenet_t struct of the node N, contains info regarding the network
 * where N belongs. It contains the gnode ID and the ipstart of each network level.
 */
typedef struct {

	uint8_t		levels;		 /* # of levels forming the net */

	/* 
	 * Array of gnode IDs				|{gidarray}|
	 *
	 * gid[0] is the node ID of N.
	 * gid[x] is the gid of the gnode of level x, belonging to this
	 * network.
	 */
	gid_t		gid[MAX_LEVELS];

	/*
	 * Each element is a pointer to the relative gnode in the ext_map.
	 */
	map_gnode	*gnode[MAX_LEVELS-ZERO_LEVEL];

	/* 
	 * The {-ipstart_t-} of each nnet.gid in their respective levels
	 */
	ipstart_t	ipstart[MAX_LEVELS];
} nodenet_t;

/*
 * Nodenet pack						|{nnet-pack}|
 *
 * It is created with the {-nnet_pack-} function.
 */
struct nodenet_pack
{
	uint8_t         levels;
	git_t		gid[MAX_LEVELS];

	/* See {-inetp-pack-} */
	struct inet_prefix_pack	 iptart_pack[MAX_LEVELS];
}_PACKED_;

/* Size of a nodenet_t pack */
#define NODENET_PACK_SZ (sizeof(struct nodenet_pack))


/* 							|{ip2nnet-flags}|
 * These are the flags passed to gmap_ip2nnet()
 */
#define NNET_IPSTART 1
#define NNET_GID     (1<<1)
#define NNET_GNODE   (1<<2)


/*\
 *
 * External map						|{extmap}|
 * ------------						|{external map}|
 *
 * The external map is composed by {-FAMILY_LVLS-}# levels.
 * A single level of the extmap is called |{single extmap}| or 
 * |{gnode map}|.
 *
 * A single extmap is an array composed by MAXGROUPNODE# {-map_gnode-} structs.
 * The i-th struct of the array corresponds to the gnode whose id is i-1.
 *
 * You can use the following functions to convert a gnode id to a struct
 * position: {-gmap_gnode2pos-}, {-gmap_pos2gnode-}.
 *
\*/
typedef struct {
	/* 
	 * An array of pointers. 
	 * gmap[x] points to the gmap of the level _NL(x).
	 */
	map_gnode	**gmap;

        /* {-MAX_METRIC_ROUTES-} value, relative to the gmaps */
        int             max_metric_routes;

	/* 
	 * This struct contains the root_gnodes of each level of the
	 * ext_map.
	 */
	nodenet_t	root_gnode;
} ext_map;

/*\
 *
 * External map pack					|{extmap_pack}|
 * =================					|{extmap pack}|
 *
 * The pack is:
 *
 *   { ext_map_hdr },
 *   {
 *   	 { intmap_pack },
 *   	 { 
 *   	     { map_gnode.flags },
 *   	     { map_gnode.seeds },
 *   	     { map_gnode.gcount }
 *   	 }^MAXGROUPNODE
 *   }^LEVELS
 *
 * Where intmap_pack is the pack of the intmap generated with {-gmap_to_map-},
 * 	 LEVELS is ext_map_hdr.nnet.levels
 *
 * To understand this syntax see {-pack-syntax-}
 *
\*/

struct ext_map_hdr
{
	/* Size of the whole pack */
	size_t		ext_map_sz;

	/* See {-MAX_METRIC_ROUTES-} */
	uint8_t         max_metric_routes;
	
	/* Packed nodenet_t of this extmap */
	struct nodenet_pack	nnet;
}_PACKED_;
INT_INFO ext_map_hdr_iinfo = { 1, { INT_TYPE_32BIT }, { 0 }, { 1 } };

/* Size of map_gnode.{flags, seeds, gcount} */
#define MAP_GNODE_EXTRA_SZ	(sizeof(u_char)+sizeof(uint8_t)+sizeof(u_int))

/*
 * Maximum size of an {-extmap pack-} 
 * `_emmr' is the {-MAX_METRIC_ROUTES-} value.
 * `_levels' is the numer of levels which compose the packed extmap.
 */
#define EXTMAP_MAX_PACK_SZ(_emmr, _levels)				\
	( sizeof(struct ext_map_hdr) + 					\
	  	(INT_MAP_PACK_SZ((_emmr), MAXGROUPNODE) * _levels) + 	\
	  	(MAP_GNODE_EXTRA_SZ * MAXGROUPNODE) )

/*\
 *
 * 	* * *  Exported functions  * * *
 *
\*/

int gmap_is_gid_invalid(gid_t *gids, gid_t gid, int lvl, int family);

/* 
 * Conversion functions 
 */

gid_t 	   gmap_gnode2pos(map_gnode *gnode, map_gnode *map);
map_gnode *gmap_pos2gnode(gid_t pos, map_gnode *map);
gid_t gmap_ip2gid(inet_prefix *ip, int level);
void  gmap_ip2gids(inet_prefix *ip, gid_t *gid, int levels);
void  gmap_gids2ip(gid_t *gid, u_char total_levels, uint8_t levels, int family,
void  gmap_ip2nnet(inet_prefix ip, map_gnode **ext_map, nodenet_t *nn, char flags);


/* 
 * {-nodenet_t-} functions 
 */
void nnet_setflags(nodenet_t *nn, char flags);
void nnet_reset(nodenet_t *nn);
void nnet_free(nodenet_t *nn);

void nnet_seeds_inc(nodenet_t *nn, int level);
void nnet_seeds_dec(nodenet_t *nn, int level);
void nnet_pack(nodenet_t *nn, char *pack);
void nnet_unpack(nodenet_t *nn, char *pack, map_gnode **extmap);

int nnet_gids_inc(nodenet_t *nn, int level,
			map_gnode **ext_map, 
			map_node *int_map, 
			int(*is_gnode_flag_set)(map_gnode *gnode), 
			int(*is_node_flag_set)(map_node *node));
int gids_find_free(nodenet_t *nn, int level, map_gnode **ext_map,
int gids_find_void(nodenet_t *nn, int level, map_gnode **ext_map,

int gmap_random_ip(inet_prefix *ipstart, int final_level, gid_t final_gid, 
			int total_levels, map_gnode **ext_map, int only_free_gnode, 
			inet_prefix *new_ip, int my_family);


int gids_cmp(gid_t *gids_a, gid_t *gids_b, int lvl, int max_lvl);
int nnet_gids_cmp(nodenet_t a, nodenet_t b, int lvl);
int inetp_gids_cmp(inet_prefix a, inet_prefix b, int lvl);

/* map_gnode */
void map_gnode_alloc(map_gnode *gnode);
void map_gnode_del(map_gnode *gnode);
void map_gnode_reset(map_gnode *gnode);

void gnode_seeds_inc(map_gnode *gnode);
void gnode_seeds_dec(map_gnode *gnode);

/* {-gnode map-} */
map_gnode *gmap_alloc(int groups);
void gmap_free(map_gnode *gmap, int groups);
void gmap_reset(map_gnode *gmap, int groups);

/* {-extmap-} */
map_gnode **extmap_alloc(u_char levels, int groups);
void extmap_free(map_gnode **ext_map, u_char levels, int groups);
void extmap_reset(map_gnode **ext_map, u_char levels, int groups);

map_gw *gmap_gw_find(map_gnode *gnode, map_gnode *n);
int  extmap_find_level(map_gnode **ext_map, map_gnode *gnode, u_char max_level);

int gmap_merge_maps(map_gnode *base, map_gnode *new, map_gnode *base_root, 
			rem_t base_new_rem,
			int new_max_metric_routes);
int extmap_merge_maps(ext_map base_map, ext_map new_map,
			rem_t base_new_rem,
			int new_max_metric_routes);

/*
 * Maps packing functions
 */

map_node *gmap_to_map(map_gnode *gmap, int count);
map_gnode *map_to_gmap(map_node *map, int count);
char *gmap_pack(map_gnode *gmap, map_gnode *root_gnode,
map_gnode *gmap_unpack(char *pack, size_t pack_sz, gid_t *root_id);

char *extmap_pack(ext_map emap, size_t *pack_sz);
ext_map extmap_unpack(char *pack, size_t pack_sz);
int extmap_verify_hdr(struct ext_map_hdr *emap_hdr);

/* Save to/Load from file */
int extmap_save(ext_map emap, char *file);
ext_map extmap_load(char *file);

#endif /*GMAP_H*/
