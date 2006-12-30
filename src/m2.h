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
 * This number must always be a power of 2.
 */
#define MAXGROUPNODE_BITS	8
#define MAXGROUPNODE		(1<<MAXGROUPNODE_BITS)

/*
 * MAX_METRIC_ROUTES
 * =================
 *
 * MAX_METRIC_ROUTES is the maximum number of different routes that can be saved
 * in a metric array (see :MetricArrays:). It can be set set at runtime. 
 * This is useful for small machine with strict memory limits.
 *
 * These conditions must be respected:
 * 	MAX_METRIC_ROUTES >= MIN_MAX_METRIC_ROUTES  
 * 	MAX_METRIC_ROUTES >= :MAX_QCACHE_ROUTES:
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
 * TODO: what to do when the counter resets?
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
 * TODO: description
 */

struct map_node
{
	u_short 	flags;		/* See :MAP_NODE_FLAGS: */

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
	 *	 ** TODO: implement this **
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
			 * It is the Tracer Packet bitmask (see :tpmask_t:) of 
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
			 * For the notion of "very similar" see :tp_almost_identical:
			 *
			 * Fuzzy hash
			 * ----------
			 *
			 * See :TODO_FUZZY_HASH:
			 */
			tpmask_t	topmask;

			/* A pointer to the map node of this gateway */
			struct map_node *node;

			/*
			 * Route Efficiency Measure (see :rem_t:) of the
			 * following route:
			 * 	this gw --> ^^map_node
			 */
			rem_t		rem;
		}
	
		/* 
		 * The metric array
		 * ================
		 *
		 * This is the metric array. It is an array of pointers of
		 * type `map_gw *'.
		 *
		 * Empty elements are set to NULL.
		 *
		 * struct map_gw*/  *gw[MAX_METRIC_ROUTES];

		/* TODO: :TODO_BSEARCH_FOR_MAP_GW: */

	} **metrics;

	RSA		*pubk;		/* Public key of the this node */
};
typedef struct map_node map_node;
typedef struct map_gw   map_gw;


#endif /* MAP_H */
