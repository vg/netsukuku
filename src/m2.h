/* This file is part of Netsukuku
 * (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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


/* 
 * NOTE: this file will substitute map.h
 */


#ifndef MAP_H
#define MAP_H

#include "inet.h"

/*
 * MAXGROUPNODE
 * ============
 *
 * Maximum number of nodes present inside a single group node.
 * This number MUST NOT be changed, there are many hardcoded parts relying on
 * MAXGROUPNODE == 256.
 */
#define MAXGROUPNODE_BITS	8
#define MAXGROUPNODE		(1<<MAXGROUPNODE_BITS)

/*
 * MAX_METRIC_ROUTES
 * =================
 *
 * MAX_METRIC_ROUTES is the maximum number of different routes that can be saved
 * in a metric array (see {-MetricArrays-}). It can be set set at runtime. 
 * This is useful for small machine with strict memory limits.
 *
 * These conditions must be respected:
 * 	MAX_METRIC_ROUTES >= MIN_MAX_METRIC_ROUTES  
 * 	MAX_METRIC_ROUTES >= {-MAX_QCACHE_ROUTES-}
 *
 * By default MAX_METRIC_ROUTES is set to DEFAULT_MAX_METRIC_ROUTES.
 */
#define DEFAULT_MAX_METRIC_ROUTES	8	
#define MIN_MAX_METRIC_ROUTES		1
int 	MAX_METRIC_ROUTES = DEFAULT_MAX_METRIC_ROUTES;


/*
 * map_node.flags 
 */
#define MAP_NODE_FLAGS
#define MAP_ME		1	/* The root node, i.e. localhost */
#define MAP_VOID	(1<<1)	/* This node doesn't exist in the net */
#define MAP_HNODE	(1<<2)  /* Hooking node. The node is hooking*/
#define MAP_BNODE	(1<<3)	/* The node is a border_node. */
#define MAP_ERNODE	(1<<4)  /* External rnode */
#define MAP_GNODE	(1<<5)  /* Group node*/
#define MAP_RNODE	(1<<6)	/* A rnode of the root node */
#define MAP_UPDATE	(1<<7)	/* If it is set, the node status changed 
				   since the last update, thus the its
				   route in the krnl should be updated */

/*
 * Link ID
 * 
 * Its 16 bits are splitted in this way:
 *
 * 	||   node id   |   link id counter   ||
 *	     8 bits           8 bits
 *
 * The overflow of the link id counter is handled with the {-counter_cmp-}
 * macro.
 */
typedef struct {
	u_short		nid:8;		/* node id */
	u_short		lid:8;		/* link id */
} linkid_t;

/* 
 * Node ID
 *
 * It's a number in [0, 255]
 */
typedef uint8_t nid_t;

#define MAX_LINKS	MAXGROUPNODE


/*
 * map_node						       |{map_node_t}|
 * --------
 *
 * A map_node struct contains all the information regarding a node of the
 * gnode of level 0.
 *
 */
struct map_node
{
	u_short 	flags;		/* See {-MAP_NODE_FLAGS-} */

	/*
	 * linkids
	 * -------
	 *
	 * Each element of the `linkids' array is the link id (see :linkid_t)
	 * of a link connecting the `self' map_node to another node of the
	 * same internal map.
	 * For this reason, the maximum number of elements is MAXGROUPNODE-1.
	 * (We substract one because `self' can never be linked with itself).
	 * Hence, using an u_char for the `links' counter is lecit.
	 */
	linkid_t	*linkids;	/* Array of link IDs */ 
	u_char		links;		/* # links of this node */

	/*
	 * MetricArrays
	 * ============
	 *
	 * Since a route can be classified in REM_METRICS different ways (rtt,
	 * upload bandwidth, ...), we keep a "metric array" for each category.
	 * In each array, we can save a maximum of MAX_METRIC_ROUTES different
	 * routes, however only their first hop is saved. The first hop is 
	 * called "gateway".
	 *
	 * A metric array is always kept sorted. Its first element is the
	 * most efficient (in terms of array's metric).
	 * 
	 * `self.metrics' is the array of all the metric arrays.
	 * `self.metrics[M].gw' is the metric array associated to the metric `M'.
	 *
	 * Note: the QSPN utilises only the first MAX_QCACHE_ROUTES routes of each
	 *       metric array.
	 *
	 * Example
	 * -------
	 *
	 * self.metrics[REM_IDX_RTT].gw is the metric array which keeps 
	 * routes sorted by their rtt. 
	 * self.metrics[REM_IDX_RTT].gw[0] is the route with the smallest
	 * rtt value.
	 *
	 * Shared gateways
	 * ---------------
	 * 
	 * self.metrics[M].gw is an array of pointers. 
	 * It may happen that a gateway is present simultaneusly in different
	 * metric arrays. For this reason, when deleting a gateway, you must
	 * be sure to delete it from all the other arrays.
	 */
	struct MetricArrays {

		/*
		 * The gateway structure
		 */
		struct map_gw
		{
			/* 
			 * tpmask
			 * =======
			 *
			 * It is the Tracer Packet bitmask (see {-tpmask_t-}) of 
			 * this gateway. It is used to discard gateways similar 
			 * to `self' from the self^^gw metric array.
			 *
			 * In this way, if the route pointed by `self' dies, then the
			 * others will have a high probability of being active.
			 *
			 * Insertion
			 * ---------
			 * 
			 * Suppose we are trying to insert the gateway G in self^^gw,
			 * then
			 *
			 * 	if    the self^^gw[j] gateway is "very similar" to G:
			 * 		the worst of the two is discarded from the 
			 * 		self^^gw array, the other is kept
			 *
			 * 	elif  self.gw isn't full:
			 * 		G is inserted in self^^gw
			 *
			 *	else:
			 *	 	G is inserted in self^^gw, and the worst
			 *	 	gateway is removed from self^^gw
			 *
			 * 	self^^gw is sorted;
			 * 
			 * For the notion of "very similar" see {-tp_similarity-}
			 *
			 * Fuzzy hash
			 * ----------
			 *
			 * See {-TODO_FUZZY_HASH-}
			 */
			tpmask_t	tpmask;

			/* A pointer to the map node of this gateway */
			struct map_node *node;

			/*
			 * Route Efficiency Measure (see {-rem_t-}) of the
			 * following route:
			 * 	root node --> this gw --> ^^map_node
			 */
			rem_t		rem;
		}
	
		/* 
		 * The metric array			   |{map_metrarray}|
		 * ================
		 *
		 * This is the metric array. It is an array of pointers of
		 * type `map_gw *'. Each pointer must be unique, i.e. there
		 * can't be two of them pointing to the same gw.
		 *
		 * The number of elements is {-MAX_METRIC_ROUTES-}.
		 * This array is allocated, once for all, at the initialization
		 * of the map, so its number of elements never change. Empty 
		 * elements are just set to NULL.
		 * All the non-empty elements are at the start of the array. 
		 * There can't be an empty element between two non-empty.
		 *
		 * --
		 *
		 * struct map_gw*/  **gw/*[MAX_METRIC_ROUTES]*/;

		/* TODO: {-TODO_BSEARCH_FOR_MAP_GW-} */

	} metrics[REM_METRICS];

	RSA		*pubkey;	/* Public key of the this node */
#define NODE_PRIVKEY_BITS	1024
#define NODE_PKEY_LEN		140
};
typedef struct map_node map_node;
typedef struct map_gw   map_gw;

/*
 * int_map
 * -------
 *
 * The internal map.
 *
 * The main part of the internal map is an array composed by 
 * MAXGROUPNODE map_node structs. The i-th struct of the array corresponds 
 * to the node whose id is `i'. The start of the array is int_map.map.
 * You can use the following functions to convert a node id to a struct
 * position: {-map_node2pos-}, {-map_pos2node-}.
 */
typedef struct {

	map_node	*map;		/* Pointer to the start of the map */

	/* {-MAX_METRIC_ROUTES-} value, relative to this map */
	int 		max_metric_routes;

	map_node	*root_node;	/* The root node, i.e. localhost */
	nid_t		root_id;	/* Root node ID */
	inet_prefix 	root_ip;	/* IP of the root node */

} int_map;

/*
 * MAP_END
 *
 * Returns the pointer to the last struct of the internal map.
 */
#define MAP_END(mapstart)	(&mapstart[MAXGROUPNODE-1])


/*\
 *
 * Internal map pack					|{intmap_pack}|
 * =================
 *
 * The internal map is packed in this way:
 *         
 *  { int_map_hdr },
 *  {
 *  	{ map_node_hdr },
 *  	{ linkid_t }^(map_node_hdr.links),
 *  	{ map_gw_pack }^(REM_METRICS*max_metric_routes)
 *  }^B
 * Where,  B = count_bits(int_map_hdr.map_mask)
 *
 * To understand this syntax see {-pack-syntax-}
 *
\*/


struct int_map_hdr
{
	size_t		int_map_sz;

	/* bit mask of the nodes included in the pack.
	 * If the i-th bit is set, then the node with ID `i' is included in
	 * the pack. */
	char		map_mask[MAXGROUPNODE/8];

	int		max_metric_routes;	/* See {-MAX_METRIC_ROUTES-} */

	nid_t		root_id;
}_PACKED_;
INT_INFO int_map_hdr_iinfo = { 2, 
			       { INT_TYPE_32BIT, INT_TYPE_32BIT }, 
			       { 0, sizeof(size_t) },
			       { 1, 1 }
			     };

struct map_node_hdr
{
	u_char		flags;
	u_char		links;		/* # of elements of the
					   node->linkids[] array */

	char		pubkey[NODE_PKEY_LEN];
}_PACKED_;
/* No INT_INFO required */

struct map_gw_pack
{
	tpmask_t        tpmask;
	nid_t		node;
	rem_t           rem;
}_PACKED_;
/* No INT_INFO required */


/*
 * INT_MAP_PACK_SZ
 * ---------------
 *
 * Returns the size of the {-intmap_pack-}.
 * `mmr' is the {-MAX_METRIC_ROUTES-} value associated to the map.
 * `mcount' is the number of nodes included in the pack.
 */
#define INT_MAP_PACK_SZ(mmr, mcount)					\
	( sizeof(struct int_map_hdr) + 					\
	  	(sizeof(map_node_hdr) + 				\
		 	(sizeof(map_gw_pack)*(REM_METRICS*(mmr))	\
		)*(mcount)						\
	)

/*
 * The int_map_block is:
 * 	struct int_map_hdr hdr;
 * 	char map_node[int_map_sz];
 * 	char map_rnode[rblock_sz];
 */
#define INT_MAP_BLOCK_SZ(int_map_sz, rblock_sz) (sizeof(struct int_map_hdr)+(int_map_sz)+(rblock_sz))

/*
 * TODO: TODO_PACK_MAP_HEADER
 */

/*\
 *
 * 	* * *  Exported functions  * * *
 *
\*/

/*
 * Conversion functions
 */
nid_t map_node2pos(map_node *node, map_node *map);
map_node *map_pos2node(nid_t pos, map_node *map);
void map_pos2ip(nid_t map_pos, inet_prefix ipstart, inet_prefix *ret);
void map_node2ip(map_node *map, map_node *node, inet_prefix ipstart, inet_prefix *ret);
int  map_ip2node(map_node *map, inet_prefix ip, inet_prefix ipstart, map_node **ret);

map_node *map_alloc(int nnodes);
void map_free(map_node *map, size_t count);
void map_reset(map_node *map, size_t count);

void map_node_reset(map_node *node);
void map_node_del(map_node *node);

int map_lid_find_nid(map_node *node, nid_t nid);
int map_lid_add(map_node *node, linkid_t id);
int map_lid_del(map_node *node, nid_t nid);

int  map_gw_add(map_node *dst, map_gw gw, map_node *root_node);
int  map_gw_del(map_node *node, map_gw *gw);
void map_gw_reset(map_node *node);
void map_gw_destroy(map_node *node);

map_gw *map_gw_find(map_node *node, map_node *n);
int     map_gw_count(map_gw **gw);
void    map_gw_sort(map_gw **gw, metric_t metric);
int     map_gw_add(map_node *dst, map_gw gw, map_node *root_node);

int map_merge_maps(map_node *base, map_node *new, map_node *base_root, 
			map_node *new_root, rem_t base_new_rem);

char *map_pack(int_map *imap, size_t *pack_sz);

#endif /* MAP_H */
