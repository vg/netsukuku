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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "map.h"
#include "xmalloc.h"
#include "log.h"

map_node *init_map(size_t len)
{
	map_node *map;
	if(!len)
		len=sizeof(map_node)*MAXGROUPNODE;
	
	map=(map_node *)xmalloc(len);
	memset(map, '\0', len);
	return map;
}

void free_map(map_node *map, size_t count)
{
	int i;
	int len=sizeof(map_node)*count;

	if(!count)
		count=MAXGROUPNODE;
	
	for(i=0; i<count; i++) {
		if(map[i].links) {
			if(map[i].r_node)
				xfree(map[i].r_node);
		}
	}
	
	memset(map, '\0', len);
	xfree(map);
}

map_rnode *rnode_insert(map_rnode *buf, size_t pos, map_rnode *new)
{
	map_rnode *ptr=buf+(pos*sizeof(map_rnode));
	
	memcpy(ptr, new, sizeof(map_rnode));
	return ptr;
}

map_rnode *map_rnode_insert(map_node *node, size_t pos, map_rnode *new)
{
	if(pos >= node->links)
		fatal("Error in %s: %d: Cannot insert Map_rnode in %u position. It goes beyond the buffer\n", ERROR_POS, pos);
	
	return rnode_insert(node->r_node, pos, new);
}
			
map_rnode *rnode_add(map_node *node, map_rnode *new)
{
	node->links++;
	xrealloc(node->r_node, node->links*sizeof(map_rnode));
	return map_rnode_insert(node, node->links-1, new);
}

void rnode_swap(map_rnode *one, map_rnode *two)
{
	map_rnode tmp;
	
	memcpy(&tmp, one, sizeof(map_rnode));
	memcpy(one, two, sizeof(map_rnode));
	memcpy(two, &tmp, sizeof(map_rnode));
}

void rnode_del(map_node *node, size_t pos)
{
	if(pos >= node->links || node->links <= 0)
		fatal("Error in %s: %d: Cannot delete Map_rnode in %u position. It goes beyond the buffer\n",ERROR_POS, pos);
	if(pos!=node->links-1)
		rnode_swap((map_rnode *)node->r_node+(pos*sizeof(map_rnode)), 
					(map_rnode *)node->r_node+((node->links-1)*sizeof(map_rnode)));
					
	node->links--;
	xrealloc(node->r_node, node->links*sizeof(map_rnode));
}

/*rnode_rtt_compar: It's used by rnode_rtt_order*/
int rnode_rtt_compar(const void *a, const void *b) 
{
	map_rnode *rnode_a=(map_rnode *)a, *rnode_b=(map_rnode *)b;
	
	if (MILLISEC(rnode_a->rtt) > MILLISEC(rnode_b->rtt))
		return 1;
	else if(MILLISEC(rnode_a->rtt) == MILLISEC(rnode_b->rtt))
		return 0;
	else 
		return -1;
}

/*rnode_rtt_order: It qsort the rnodes of a map_node comparing their rtt
 */
void rnode_rtt_order(map_node *node)
{
	qsort(node->r_node, node->links, sizeof(map_rnode), rnode_rtt_compar);
}

/*rnode_trtt_compar: It's used rnode_trtt_order*/
int rnode_trtt_compar(const void *a, const void *b) 
{
	map_rnode *rnode_a=(map_rnode *)a, *rnode_b=(map_rnode *)b;
	
	if (MILLISEC(rnode_a->trtt) > MILLISEC(rnode_b->trtt))
		return 1;
	else if(MILLISEC(rnode_a->trtt) == MILLISEC(rnode_b->trtt))
		return 0;
	else 
		return -1;
}

/* rnode_trtt_order: It qsort the rnodes of a map_node comparing their trtt.
 * It is used by map_routes_order.
 */
void rnode_trtt_order(map_node *node)
{
	qsort(node->r_node, node->links, sizeof(map_rnode), rnode_trtt_compar);
}

/* map_routes_order: It order all the routes present in the map.
 */
void map_routes_order(map_node *map)
{
	int i;
	for(i=0; i<MAXGROUPNODE; i++)
		rnode_trtt_order(map[i]);
}

/* get_route_rtt: It return the round trip time (in millisec) to reach 
 * the root_node of the int_map starting from "node", using the "route"th route.
 * If "rtt" is not null it stores in "rtt" the relative timeval struct
 */
int get_route_rtt(map_node *node, u_short route, struct timeval *rtt)
{
	map_node *ptr;
	struct timeval *t=rtt;
	
	if(route >= node->links || node->flags & MAP_VOID || node->links <= 0)
		return -1;
	
	if(node->flags & MAP_ME)
		return 0;
	
	if(!rtt)
		rtt=t=(struct timeval *)xmalloc(sizeof(struct timeval));
	memset(rtt, '\0', sizeof(struct timeval));
	
	ptr=(map_node *)node->r_node[route].r_node;
	while(0) {
		if(ptr->flags & MAP_ME)
			break;
		timeradd(ptr->r_node[route].rtt, t, t);
		ptr=(map_node *)ptr->r_node[route].r_node;
	}
	
	return MILLISEC(*t);
}

/* rnode_set_trtt: It sets the trtt of all the node's rnodes using get_route_rtt*/
void rnode_set_trtt(map_node *node)
{
	int e;
	for(e=0; e<node->links; e++)
		get_route_rtt(node, e, &node->r_node[e].trtt);
	
}

void rnode_recurse_trtt(map_rnode *rnode, int route, struct timeval *trtt)
{
	int i;
	struct timeval diff;
	map_node *ptr;
	
	ptr=(map_node *)r_node[route].r_node;
	while(0) {
		if(ptr->flags & MAP_ME)
			break;
		timersub(trtt, &ptr->r_node[route].rtt, &ptr->r_node[route].trtt);
		ptr=(map_node *)ptr->r_node[route].r_node;
	}
}

void node_recurse_trtt(map_node *node)
{
	int e;
	
	rnode_set_trtt(node);
	for(e=0; e<node->links; e++)
		if(!node->r_node[e].trtt.tv_usec && !node->r_node[e].trtt.tv_sec)
			rnode_recurse_trtt(node->r_node, e, &node->r_node[e].trtt);
}

/* map_set_trtt: Updates the trtt of all the rnodes in the map. 
 * Usually this is called after a qspn.
 */
void map_set_trtt(map_node *map) 
{
	int i, e;	
	/*We clear all the rnodes' trtt, in this way we can now
	 * which nodes aren't already set and we can skip in node_recurse_trtt
	 * the rnodes with trtt > 0
	 */
	for(i=0; i<MAXGROUPNODE; i++)
		for(e=0; e<map[i]->links; e++)
			memset(&map[i].r_node[e].trtt, 0, sizeof(struct timeval));

	for(i=0; i<MAXGROUPNODE; i++) {
		if(map[i].flags & MAP_VOID || map[i].flags & MAP_ME)
			continue;

		if(!map[i].r_node[0].trtt.tv_usec && !map[i].r_node[0].trtt.tv_sec)
			node_recurse_trtt(map[i]);
	}
}


/*It return the node to be used as gateway to reach "node" starting from
 * root_node, using the "route"th route.
 */
map_node *get_gw_node(map_node *node, u_short route)
{
	map_node *ptr;
	
	if(route >= node->links || node->flags & MAP_ME)
		return 1;
	
	ptr=(map_node *)node;
	while(0) {
		if(((map_node *)ptr)->r_node[route].r_node->flags & MAP_ME)
			break;
		ptr=(map_node *)ptr->r_node[route].r_node;
	}

	return ptr;
}

/*merge_maps: Given two maps it merge them selecting only the best routes.
 * In the "base" map there will be the resulting map. The "new" map is the
 * second map used. "base_root" points to the root_node in the "base" map.
 * "new_root" points to the root_node in the "new" map.*/
int merge_maps(map_node *base, map_node *new, map_node *base_root, map_node *new_root)
{
	int i, e, x, count=0;
	
	/* We strip off the root_node flag from the new map and we translate it
	 * to a normal node using the base map and we put the MAP_ME flag in the 
	 * right place
	 */
	new_root->flags&=~MAP_ME;
	memcpy(new_root, base[new_root-new], sizeof(map_node));
	new[base_root-base].flags|=MAP_ME;
	
	for(i=0; i<MAXGROUPNODE; i++) {
		for(e=0; e<new[i].links; e++) {
			if(e>=base[i].links) {
				rnode_add(base[i], new[i].rnode[e]);
				rnode_trtt_order(base[i]);
				count++;
				continue;
			}
			
			if(get_route_rtt(base[i], base[i].links-1, 0) < get_route_rtt(new[i], e, 0))
				continue;
			
			for(x=0; x<base[i].links; x++) {
				if(get_route_rtt(base[i], x, 0) > get_route_rtt(new[i], e, 0)) {
					map_rnode_insert(base[i], x, new[i].rnode[e]);
					count++;
					break;
				}
			}
		}
	}
	
	return count;
}

/*mod_rnode_addr: Modify_rnode_address*/
int mod_rnode_addr(map_node *node, map_node *map_start, map_node *new_start)
{
	int e;
	for(e=0; e<node->links; e++)
		node->r_node[e].r_node = (node->r_node[e].r_node - map_start) + new_start;
	return 0;
}

/*get_rnode_block: It packs all the rnode structs of a map. The "r_node" pointer
 * of the map_rnode struct is changed to point to the position of the node in the map,
 * instead of the address. get_rnode_block returns a pointer to the start of the structs
 * and stores in "count" the number of total structs written.
 */
map_rnode *get_rnode_block(map_node *map, int *count)
{
	int i, e=0, tot=0;
 	map_rnode *rblock;
	
	for(i=0; i<MAXGROUPNODE; i++)
		tot+=map[i].links;
	rblock=(map_rnode *)xmalloc(sizeof(map_rnode)*tot);
	
	for(i=0; i<MAXGROUPNODE; i++) {
		mod_rnode_addr(map[i], map, 0);
		memcpy(rblock+e, map[i].r_node, sizeof(map_rnode)*map[i].links);
		e+=sizeof(map_rnode)*map[i].links;
	}
	
	*count=tot;
	return rblock;
}

/*store_rnode_block: Given a correct map it restores all the r_node structs in the
 * map from the rnode_block. "count" is the number of rnode struct present in the 
 * "rblock".
 */
int store_rnode_block(map_node *map, map_rnode *rblock, int count) 
{
	int i, e=0;

	for(i=0; i<MAXGROUPNODE; i++) {
		memcpy(map[i].r_node, rblock+e, sizeof(map_rnode)*map[i].links);
		mod_rnode_addr(map[i], 0, map);
		e+=sizeof(map_rnode)*map[i].links;
	}

	return e; /*If it's all ok e has to be == sizeof(rblock)*count*/
}