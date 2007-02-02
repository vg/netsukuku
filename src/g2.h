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

/*\
 * |{mapgnode_pack}|
 * TODO:
\*/
INT_INFO map_gnode_iinfo = { 1, 
			     { INT_TYPE_32BIT }, 
			     { IINFO_DYNAMIC_VALUE },
			     { 1 }
			   };
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

/*\
 *
 * External map						|{extmap}|
 * ------------
 *
 * The external map is composed by {-FAMILY_LVLS-}# levels.
 * Each level is called |{single extmap}|.
 *
 * A single extmap is an array composed by MAXGROUPNODE# map_gnode structs.
 * The i-th struct of the array corresponds to the gnode whose id is `i'.
 *
 * You can use the following functions to convert a gnode id to a struct
 * position: {-gmap_gnode2pos-}, {-gmap_pos2gnode-}.
 *
\*/

/*
 * 			      Levels notes
 * 			    ================
 * 			
 * These are the levels of the external map. 
 *
 * The |{ZERO LEVEL}| level is never used in the ext_map because it corresponds to the
 * internal map.
 *
 * The UNITY_LEVEL is the symbolic level which contains only one gnode, i.e.
 * the gnode containing the entire Netsukuku. This level isn't used, 
 *
 * The EXTRA_LEVELS are the ZERO_LEVEL and the UNITY_LEVEL.
 *
 * |{EL_macro}|
 * All the arrays of levels related to the external map, and the ext_map itself, don't
 * use the EXTRA_LEVELS. For this reason, they lack of the zero level.
 * This means that the level 1 is in the position 0 of the array,
 * level 2 in 1, and so on.
 * To simplify the access to the array use the {-_EL-} macro:
 * 	ext_map[_EL(1)]  <--  access level 1 of the external map
 * which is the same of:
 * 	ext_map[0]
 *
 * Some of these arrays of levels are: nnet.gnode, rblock, ext_map, qspn_gnode_count.
 */
#define ZERO_LEVEL	1
#define UNITY_LEVEL	1
#define EXTRA_LEVELS	(ZERO_LEVEL + UNITY_LEVEL)

/* To use the right level. See {-EL_macro-} */
#define _EL(level)    ((level)-1)
/* And to restore it. */
#define _NL(level)    ((level)+1)

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
 * currently used family.
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
 *
 */
typedef struct {

	uint8_t		levels;		 /* # of levels forming the net */

	/* 
	 * Array of gnode IDs				|{gidarray}|
	 *
	 * The member gid[x] is the gid of the gnode of level x, belonging to
	 * this network.
	 */
	gid_t		gid[MAX_LEVELS];

	/*
	 * Each element is a pointer to the relative gnode in the ext_map.
	 */
	map_gnode	*gnode[MAX_LEVELS-ZERO_LEVEL];

	/* 
	 * The ipstart of each nnet.gid in their respective levels
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

/* This block is used to send the ext_map */
struct ext_map_hdr
{
	char   nnet[NODENET_PACK_SZ];  /* The packed me.cur_nnet */

	size_t ext_map_sz; 		/*It's the sum of all the gmaps_sz.
					  The size of a single map is:
					  (ext_map_sz/(MAP_GNODE_PACK_SZ*
					  (nnet.levels-EXTRA_LEVELS)); */
	size_t rblock_sz[MAX_LEVELS];	/*The size of the rblock of each gmap*/
	size_t total_rblock_sz;		/*The sum of all rblock_sz*/
}_PACKED_;

/* Note: You have to consider the nodenet_t struct when converting between
 * endianness */
INT_INFO ext_map_hdr_iinfo = { 3, 
			       { INT_TYPE_32BIT, INT_TYPE_32BIT, INT_TYPE_32BIT },
			       { NODENET_PACK_SZ, 
				   NODENET_PACK_SZ+sizeof(size_t),
				   NODENET_PACK_SZ+(sizeof(size_t)*(MAX_LEVELS+1)) },
			       { 1, MAX_LEVELS, 1 }
			     };
	
/* The ext_map_block is:
 * 	struct ext_map_hdr hdr;
 * 	char ext_map[ext_map_sz];
 * 	char rnode_blocks[total_rblock_sz];
 */
#define EXT_MAP_BLOCK_SZ(ext_map_sz, rblock_sz) (sizeof(struct ext_map_hdr)+(ext_map_sz)+(rblock_sz))

/* 
 * This struct is used by the root_node to describe all the rnodes which
 * doesn't belongs to our same gnode.
 */
typedef struct {
	map_node	node;
	nodenet_t 	nnet;	/* nnet.gnode[level] may be set to 0
				 * if that gnode doesn't belong to the
				 * same upper level of me.cur_nnet:
				 * nnet.gid[level+1] != me.cur_nnet.gid[level+1]
				 */
}ext_rnode;

/*This cache keeps the list of all the ext_rnode used.*/
struct ext_rnode_cache {
	LLIST_HDR	(struct ext_rnode_cache);

	ext_rnode	*e;		/*The pointer to the ext_rnode struct*/
	int		rnode_pos;	/*The ext_rnode position in the 
					  array of rnodes of the root_node */
};
typedef struct ext_rnode_cache ext_rnode_cache;

/* * * Functions' declaration * * */

#endif /*GMAP_H*/
