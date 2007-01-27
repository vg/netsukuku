/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 *
 * --
 * Internal map code.
 *
 * For more information on the maps and the topology of Netsukuku see
 * {-CITE_TOPOLOGY_DOC-}
 */

#include "includes.h"

#include "rem.h"
#include "map.h"
#include "common.h"

extern int errno;

/*
 * map_node2pos
 * ------------
 *
 * Converts a map_node pointer `node' to the position of the pointed struct 
 * in the internal map.
 * This position corresponds to the node id (see {-map_node_t-})
 *
 * `map' must be a pointer to the first struct of the internal map.
 */
nid_t map_node2pos(map_node *node, map_node *map)
{
	return ((char *)node-(char *)map)/sizeof(map_node);
}

/*
 * map_pos2node
 * ------------
 *
 * The inverse of {-map_node2pos-}
 *
 * `map' must be a pointer to the first struct of the internal map.
 */
map_node *map_pos2node(nid_t pos, map_node *map)
{
	return (map_node *)((pos*sizeof(map_node))+(char *)map);
}

/* 
 * map_pos2ip
 * ----------
 * 
 * `map_pos' is the position of a map_node struct in the internal map, it is
 * also a node's id (see {-map_node_t-}).
 * This function converts `map_pos' to the IP of the node, which is then saved 
 * in `ret'.
 *
 * The localhost `ipstart' of level 1 is also needed (see {-ipstart_t-})
 */
void map_pos2ip(nid_t map_pos, inet_prefix ipstart, inet_prefix *ret) 
{
	if(ipstart.family==AF_INET) {
		ret->data[0]=map_pos + ipstart.data[0];
		ret->len=4;
	} else {
		ret->len=16;
		sum_int(map_pos, ret->data);
	}
	inet_copy_ipdata_raw(ret->data, &ipstart);
	ret->data[0]= ((ret->data[0] >> 8) << 8) | map_pos;
	ret->family = ipstart.family;
	ret->len    = ipstart.family==AF_INET ? 4 : 16;
	ret->bits   = ret->len*8;
}

/*
 * map_node2ip
 * -----------
 *
 * `node' is a pointer to a map_node struct.
 * This function converts `node' to the IP of the relative node. The IP is
 * then saved in `ret'.
 *
 * `map' must be a pointer to the first struct of the internal map.
 * `ipstart' must be the localhost ipstart of level 1 (see {-ipstart_t-}).
 */
void map_node2ip(map_node *map, map_node *node, inet_prefix ipstart, inet_prefix *ret);
{
	map_pos2ip(map_node2pos(node, map), ipstart, ret);
}

/*
 * map_ip2node
 * -----------
 *
 * The inverse of {-map_node2ip-}
 * It converts `ip' to a pointer `*ret' pointing to the relative node of
 * level 0.
 * For example, if the IP is 11.22.33.44, then this functions returns 
 * &map[44], where `map' is the pointer to the first struct of the internal
 * map.
 *
 * `ipstart' must be the localhost ipstart of level 1 (see {-ipstart_t-}).
 *
 * If the converted node doesn't belong to the localhost's gnode of level 1, 
 * then 1 is returned.
 */
int map_ip2node(map_node *map, inet_prefix ip, inet_prefix ipstart, map_node **ret)
{
	u_char *h_ip, *h_node, *i_ip;
	int ipsz;
	
	h_ip = (u_char *)ip.data;
	i_ip = (u_char *)ipstart.data;
	ipsz = ip.family==AF_INET ? INET_IPV4_SZ : INET_IPV6_SZ;

#if BYTE_ORDER == LITTLE_ENDIAN
	h_node=&h_ip[0];
#else
	h_node=&h_ip[ipsz-1];
#endif
	*ret=&map[(nid_t)(*h_node)];

	for(i=1; i<ipsz; i++)
#if BYTE_ORDER == LITTLE_ENDIAN
		if(h_ip[i] != i_ip[i])
#else
		if(h_ip[ipsz-i-1] != i_ip[i])
#endif
			return 1

	return 0;
}



/*
 * map_alloc
 * ---------
 *
 * Allocates an array of `nnodes' map_node structs and returns a pointer to
 * the first element. If `nnodes' == 0, then it allocates MAXGROUPNODE
 * structs.
 *
 * The map_node.flags of each element is set to MAP_VOID.
 * The arrays of map_gw pointers are allocated too.
 * Whatever is allocated is set to zero.
 */
map_node *map_alloc(int nnodes)
{
	int i;
	map_node *map;

	if(!nnodes)
		nnodes = MAXGROUPNODE;

	map = xzalloc(nnodes*sizeof(map_node));
	for(i=0; i<nnodes; i++) {
		map[i].flags|=MAP_VOID;
		int e;
		
		/* Allocate the metric arrays */
		for(e=0; e<REM_METRICS; e++) {
			int nmemb=MAX_METRIC_ROUTES;
			array_grow(&map[i].metrics[e].gw, &nmemb, -1, 0);
			array_bzero(&map[i].metrics[e].gw, &nmemb, -1);
		}
	}

	return map;
}

/*
 * map_free
 * --------
 * 
 * Destroy the `map', which has `count' elements.
 * If `count' is zero, it supposes that the `map' has MAXGROUPNODE elements.
 */
void map_free(map_node *map, size_t count)
{
	int i;

	if(!count)
		count=MAXGROUPNODE;

	for(i=0; i<count; i++)
		map_node_free(&map[i]);

	xfree(map);
}

/*
 * map_reset
 * ---------
 * 
 * Calls {-map_node_reset-} for the first `count' elements of the `map'.
 * If `count' is zero, it supposes that the `map' has MAXGROUPNODE elements.
 */
void map_reset(map_node *map, size_t count)
{
	int i;

	if(!count)
		count=MAXGROUPNODE;

	for(i=0; i<count; i++)
		map_node_reset(&map[i]);
}

/*
 * map_node_reset
 * --------------
 *
 * Reset the map_node structure pointed by `node' and keeps allocated some
 * arrays which might be useful in future. If you want to free every single
 * allocated resource use {-map_node_del-}
 *
 * The `node.flags' is set to MAP_VOID, to indicate that this node is now
 * empty.
 *
 * It frees `node.linkids'.
 * It frees each map_gw referenced in the `node.metrics[*].gw' arrays, but 
 * it doesn't deallocate the `node.metrics[*].gw' arrays of pointers.
 * The `node.pubkey' is also deallocated.
 */
void map_node_reset(map_node *node)
{
	array_destroy(&node.linkids, &node.links, 0);

	map_gw_reset(node);

	if(node.pubkey)
		RSA_free(node.pubkey);

	setzero(node, sizeof(map_node));
	node.flags|=MAP_VOID;
}

/*
 * map_node_del
 * ------------
 *
 * The same of {-map_node_reset-}, but deallocates every allocated resource
 */
void map_node_del(map_node *node)
{
	int e;

	map_node_reset(node);
	map_gw_destroy(node);
}

/*
 * map_gw_del
 * ----------
 *
 * Deletes the gateway pointed by `gw' from the map_node `node' and deallocates
 * it.
 * If nothing is deleted zero is returned.
 */
int map_gw_del(map_node *node, map_gw *gw)
{
	int ret=0, e;

	!gw && _return(ret);

	/*
	 * See the "Shared gateways" notes in {-MetricArrays-} to
	 * understand why we are using this loop.
	 */
	for(e=0; e<REM_METRICS; e++) {

		int x;

		x=map_metr_gw_find(node, e, gw.node);
		if(x == -1)
			continue;

		int nmemb=MAX_METRIC_ROUTES;

		/* If it's the first time we are deleting
		 * `gw', deallocate it too. */
		!ret && zfree(node.metrics[e].gw[x]);

		/* Remove the pointer from the metric array */
		array_rem(&node.metrics[e].gw, &nmemb, x);

		/* Set to zero the pointer left by the
		 * shifting of the array */
		node.metrics[e].gw[nmemb]=0;

		ret+=1;
		break; /* We're assuming there's only 
			  one gw in each metric array */
	}

	return ret;
}

/*
 * map_metr_gw_del
 * ---------------
 *
 * Deletes the gateway pointed by `gw' from the metric array
 * `node'.metrics[`metric']. 
 * `gw' will be deallocated if it was used only in the specified metric array.
 *
 * It returns 1 if it has been deleted, otherwise 0.
 */
int map_metr_gw_del(map_node *node, int metric, map_gw *gw)
{
	int x;
	x=map_metr_gw_find(node, e, gw.node);
	if(x == -1)
		return 0;

	int nmemb=MAX_METRIC_ROUTES;

	/* Remove the pointer from the metric array */
	array_rem(&node.metrics[metric].gw, &nmemb, x);

	/* Set to zero the pointer left by the
	 * shifting of the array */
	node.metrics[metric].gw[nmemb]=0;

	if(!map_gw_find(node, gw.node))
		/* We can deallocate `gw' since it was used
		 * only in the metric `metric'. */
		zfree(gw);

	return 1;
}

/*
 * map_gw_reset
 * ------------
 *
 * It frees each map_gw referenced in the `node.metrics[*].gw' arrays, but it
 * doesn't deallocate the `node.metrics[*].gw' arrays of pointers.
 */
void map_gw_reset(map_node *node)
{
	int e;
	for(e=0; e < REM_METRICS; e++)
		while(node.metrics[e].gw[0])
			map_gw_del(node, node.metrics[e].gw[x]);
}

/*
 * map_gw_destroy
 * --------------
 *
 * Like {-map_gw_reset-}, but it deallocate the `node.metrics[*].gw' arrays of
 * pointers too.
 */
void map_gw_destroy(map_node *node)
{
	map_gw_reset(node);

	int e;
	for(e=0; e < REM_METRICS; e++)
		array_destroy(&node.metrics[e].gw, 0, 0);
}

/*
 * map_gw_find
 * -----------
 *
 * It searches, inside the metric array `node'.metrics[`metric'],
 * a gw which points to the node `n'.
 * It then returns the array position of the gw.
 * If it isn't found -1, is returned.
 */
int map_metr_gw_find(map_node *node, int metric, map_node *n)
{
	int i;

	for(i=0; i < MAX_METRIC_ROUTES; i++)
		if(node.metrics[e].gw[i]->node == n)
			return i;
	return -1;
}

/*
 * map_gw_find
 * -----------
 *
 * It searches in the `node' a gw which points to the node `n'.
 * It then returns the pointer to that gw.
 * If it is not found it returns 0;
 */
map_gw *map_gw_find(map_node *node, map_node *n)
{
	int e, i;
	for(e=0; e < REM_METRICS; e++)
		if((i=map_metr_gw_find(node, e, n) != -1))
			return node.metrics[e].gw[i];
	return 0;
}

/*
 * map_gw_count
 * ------------
 *
 * Returns the number of non zero pointers present at the start of the `gw'
 * array.
 */
int map_gw_count(map_gw **gw)
{
	int i;
	for(i=0; i < MAX_METRIC_ROUTES && gw[i]; i++);
	return i;
}

/*
 * map_gw_sort
 * -----------
 *
 * Sorts, in decrescent order of efficiency, the `gw' metric array 
 * using the specified `metric'.
 * 
 * See {-map_metrarray-}, {-metric_t-}.
 */
void map_gw_sort(map_gw **gw, metric_t metric)
{
	if(!gw[0])
		return;

	int map_gw_cmp(const void *a, const void *b)
	{
		return -rem_metric_cmp(a.rem, b.rem, metric);
	}

	qsort(gw, map_gw_count(gw), sizeof(map_gw *), map_gw_cmp);
}

/*
 * map_gw_add
 * ----------
 *
 * Adds `gw' in the list of gateways used to reach the `dst' node.
 *
 * `root_node' must point to the root node of the internal map.
 *
 * Returns 1 on success, 0 if the `gw' has been discarded.
 */
int map_gw_add(map_node *dst, map_gw gw, map_node *root_node)
{
	if(gw.node == root_node || dst == root_node)
		/* Invalid call */
		return 0;

	map_gw *newgw;
	int e, ret=0;

	/* Check if the `gw' has been added before */
	newgw = map_gw_find(dst, gw.node);

	for(e=0; e<REM_METRICS; e++) {

		int j, skip_metric=-1;

		for(j=0; j < MAX_METRIC_ROUTES; j++) {
			if(!dst.metrics[e].gw[j])
				break;

			if(skip_metric && 
				tp_is_similar(gw.tpmask, dst.metrics[e].gw[j]->tpmask)) {

				/* 
				 * `gw' is very similar to `gw[j]'. Delete the
				 * worst and keep the best
				 */
				int cmp;
				cmp = rem_metric_cmp(gw.rem, dst.metrics[e].gw[j]->rem, e);

				if(cmp <= 0) {
					/* gw.rem is worse than, or equal to gw[j].
					 * Discard `gw' from this metric array. */
					skip_metric=1;
					break;
				} else {
					/* gw.rem is better than gw[j].
					 * Delete gw[j], `gw' will replace
					 * it. */
					if(map_metr_gw_del(dst, e, dst.metrics[e].gw[j]))
						/* In the current position, there's now
						   a different gw */
						j--; 

					skip_metric=0;
				}
			}
		}
		if(skip_metric > 0)
			continue;

		/* `j' is now equal to the number of gateways present in the
		 * array */

		if(j > MAX_METRIC_ROUTES) {
			/* The array is full. Delete the worst gateway */
			int cmp;

			j=MAX_METRIC_ROUTES-1;
			cmp=rem_metric_cmp(gw.rem, dst.metrics[e].gw[j]->rem, e);

			if(cmp <= 0)
				/* gw.rem is worse than, or equal to gw[j],
				 * that is, in this case, the worst gw.
				 * Discard `gw' from this metric array. */
				continue;
			else
				/* `gw' is better than gw[j]. Delete gw[j]. */
				map_metr_gw_del(dst, e, dst.metrics[e].gw[j]);
		}

		if(!newgw) {
			newgw=xmalloc(sizeof(map_gw));
			memcpy(newgw, &gw, sizeof(map_gw));
		}

		/* Add the new gw */
		int nalloc=MAX_METRIC_ROUTES;
		array_add(&dst.metrics[e].gw, &j, &nalloc, newgw);
		ret=1;
	}

	return ret;
}


/*
 * map_merge_maps
 * --------------
 *
 * Given two maps it merges them selecting only the best routes.
 *
 * In `base' map there will be the resulting map. 
 * The `new' map is the second map.
 * `base_root' points to the root_node present in the `base' map.
 * `new_root' points to the root_node of the `new' map.
 *
 * `base_new_rem' is the {-rem_t-} of the link `base'<-->`new'.
 *
 * * WARNING *
 * It's assumed that `new_root' is a rnode of `base_root'.
 * * WARNING *
 */
int map_merge_maps(map_node *base, map_node *new, map_node *base_root, 
			map_node *new_root, rem_t base_new_rem)
{
	/* Vars */
	int i, e, x, count=0, base_root_pos, new_root_pos;
	map_gw gwstub, *gwnew;

	/* Code */

	base_root_pos= map_node2pos(base_root, base);
	new_root_pos = map_node2pos(new_root, new);
	gwstub.node=&base[new_root_pos];

	/* forall node in `new' */
	for(i=0; i<MAXGROUPNODE; i++) {

		if(new[i].flags & MAP_ME || new[i].flags & MAP_VOID)
			continue;

		for(e=0; e < REM_METRICS; e++)
		    for(x=0; x < MAX_METRIC_ROUTES && new[i].metrics[e].gw[x]; x++) {

			gwnew = new[i].metrics[e].gw[x];

			/*
			 * Migrate the tpmask to the new map
			 */
			memcpy(&gwstub.tpmask, &gwnew->tpmask, sizeof(tpmask_t));
			if(tp_mask_test(&gwstub.tpmask, base_root_pos))
				/* This route uses base_root_pos as a hop,
				 * thus discard it. */
				continue;

			tp_mask_set(&gwstub.tpmask, new_root_pos, 1);

			/*
			 * Migrate the REM value
			 */
			memcpy(&gwstub.rem, &gwnew->rem, sizeof(rem_t));
			/* gwstub.rem = gwstub.rem + base_new_rem */
			rem_add(&gwstub.rem, gwstub.rem, base_new_rem);

			/* Add the gw in `base' */
			count+=map_gw_add(&base[i], gwstub, base_root);
		    }
	}

	return count;
}
