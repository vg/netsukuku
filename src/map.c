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
 *
 * --
 * Internal map code.
 */

#include "includes.h"

#include "inet.h"
#include "map.h"
#include "ipv6-gmp.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"

extern int errno;

/*
 * pos_from_node: Position from node: It returns the position of the `node'
 * in the `map'.
 */
int pos_from_node(map_node *node, map_node *map)
{
	return ((void *)node-(void *)map)/sizeof(map_node);
}

/*
 * Node from position: it returns the node pointer calculated by the given 
 * `pos' in the map.
 */
map_node *node_from_pos(int pos, map_node *map)
{
	return (map_node *)((pos*sizeof(map_node))+(void *)map);
}

/* 
 * Position (of a struct in the map) to ip: Converts the node position 
 * `map_pos' to its relative ip.
 */
void postoip(u_int map_pos, inet_prefix ipstart, inet_prefix *ret) 
{
	if(ipstart.family==AF_INET) {
		ret->data[0]=map_pos + ipstart.data[0];
		ret->family=AF_INET;
		ret->len=4;
	} else {
		ret->family=AF_INET6;
		ret->len=16;
		memcpy(ret->data, ipstart.data, sizeof(inet_prefix));
		sum_int(map_pos, ret->data);
	}
	ret->bits=ret->len*8;
}

/* 
 * Map (address) to ip: Converts an address of a struct in the map to the
 * corresponding ip.
 */
void maptoip(u_int mapstart, u_int mapoff, inet_prefix ipstart, inet_prefix *ret)
{
	int map_pos=pos_from_node((map_node *)mapoff, (map_node *)mapstart);
	postoip(map_pos, ipstart, ret);
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

		/* h_ipstart=h_ip - h_ipstart */
		sub_128(h_ip, h_ipstart);
		/* The result is always < MAXGROUPNODE, so we can take for grant that
		 * we have only one u_int*/
		*ret=h_ipstart[0]*sizeof(map_node)+mapstart;
	}

	if(*ret > INTMAP_END(mapstart) || *ret < mapstart)
		/*Ok, this is an extern ip to our gnode.*/
		return 1;

	return 0;
}

map_node *init_map(size_t len)
{
	int i;
	map_node *map;
	if(!len)
		len=sizeof(map_node)*MAXGROUPNODE;
	
	map=(map_node *)xmalloc(len);
	memset(map, '\0', len);
	for(i=0; i<MAXGROUPNODE; i++)
		map[i].flags|=MAP_VOID;
	
	return map;
}

void free_map(map_node *map, size_t count)
{
	int i, len;

	if(!count)
		count=MAXGROUPNODE;
	len=sizeof(map_node)*count;
	
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
	map_rnode *ptr=buf+pos;
	
	memcpy(ptr, new, sizeof(map_rnode));
	return ptr;
}

map_rnode *map_rnode_insert(map_node *node, size_t pos, map_rnode *new)
{
	if(pos >= node->links)
		fatal("Error in %s: %d: Cannot insert map_rnode in %u position. It goes beyond the buffer\n", ERROR_POS, pos);
	
	return rnode_insert(node->r_node, pos, new);
}
			
map_rnode *rnode_add(map_node *node, map_rnode *new)
{
	node->links++;
	if(node->links == 1)
		node->r_node=xmalloc(sizeof(map_rnode));
	else
		node->r_node=xrealloc(node->r_node, node->links*sizeof(map_rnode));
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
		rnode_swap((map_rnode *)&node->r_node[pos], (map_rnode *)&node->r_node[(node->links-1)]);
					
	node->links--;
	if(!node->links) {
		xfree(node->r_node);
		node->r_node=0;
	} else
		node->r_node=xrealloc(node->r_node, node->links*sizeof(map_rnode));
}

/* rnode_destroy: Wipe out all the rnodes YEAHAHA ^_- */
void rnode_destroy(map_node *node)
{
	if(node->r_node && node->links)
		xfree(node->r_node);
	node->r_node=0;
	node->links=0;
}

/* rnode_find: It searches in the `node' a rnode which points to the node `n'.
 * It then returns the position of that rnode.
 * If the rnode is not found it returns -1;*/
int rnode_find(map_node *node, map_node *n)
{
	int e;
	for(e=0; e < node->links; e++)
		if((map_node *)node->r_node[e].r_node == n)
			return e;
	return -1;
}


/* map_node_del: It deletes a `node' from the `map'. Really it frees its rnodes and 
 * set the node's flags to MAP_VOID.*/
void map_node_del(map_node *node)
{
	rnode_destroy(node);
	memset(node, 0, sizeof(map_node));
	node->flags|=MAP_VOID;
}

void reset_int_map(map_node *map, int maxgroupnode)
{
	int i;
	
	if(!maxgroupnode)
		maxgroupnode=MAXGROUPNODE;
	
	for(i=0; i<maxgroupnode; i++)
		map_node_del(&map[i]);
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

/*rnode_trtt_compar: It's used by rnode_trtt_order*/
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

/* map_routes_order: It order all the r_node of each node using their trtt.
 * Used mainly with a qspn map styleII */
void map_routes_order(map_node *map)
{
	int i;
	for(i=0; i<MAXGROUPNODE; i++)
		rnode_trtt_order(&map[i]);
}

/* get_route_rtt: It return the round trip time (in millisec) to reach 
 * the root_node of the int_map starting from "node", using the "route"th route.
 * If "rtt" is not null it stores in "rtt" the relative timeval struct
 * (qspn map styleI) */
int get_route_rtt(map_node *node, u_short route, struct timeval *rtt)
{
	map_node *ptr;
	struct timeval *t=rtt;
	
	if(route >= node->links || node->flags & MAP_VOID || node->links <= 0)
		return -1;
	
	if(!rtt)
		rtt=t=(struct timeval *)xmalloc(sizeof(struct timeval));
	memset(rtt, '\0', sizeof(struct timeval));
	
	if(node->flags & MAP_ME)
		return 0;
	
	ptr=(map_node *)node->r_node[route].r_node;
	while(1) {
		if(ptr->flags & MAP_ME)
			break;
		timeradd(&ptr->r_node[route].rtt, t, t);
		ptr=(map_node *)ptr->r_node[route].r_node;
	}
	
	return MILLISEC(*t);
}

/* get_route_trtt: It's the same of get_route_rtt, but it returns the 
 * totatl round trip time (trtt).
 * It's mainly used in the qspn_map styleII*/
int get_route_trtt(map_node *node, u_short route, struct timeval *trtt)
{
	struct timeval *t=trtt;
	
	if(route >= node->links || node->flags & MAP_VOID || node->links <= 0)
		return -1;
	
	if(!trtt)
		trtt=t=(struct timeval *)xmalloc(sizeof(struct timeval));
	memset(trtt, '\0', sizeof(struct timeval));
	
	if(node->flags & MAP_ME)
		return 0;
	
	memcpy(t, &node->r_node[route].trtt, sizeof(struct timeval));
	return MILLISEC(node->r_node[route].trtt);
}

/* rnode_set_trtt: It sets the trtt of all the node's rnodes using get_route_rtt.
 * (qspn map styleI)*/
void rnode_set_trtt(map_node *node)
{
	int e;
	for(e=0; e<node->links; e++)
		get_route_rtt(node, e, &node->r_node[e].trtt);
	
}


void rnode_recurse_trtt(map_rnode *rnode, int route, struct timeval *trtt)
{
	map_node *ptr;
	
	ptr=(map_node *)rnode[route].r_node;
	while(1) {
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

/* map_set_trtt: Updates the trtt of all the rnodes in a qspn map styleI.*/
void map_set_trtt(map_node *map) 
{
	int i, e;	
	/*We clear all the rnodes' trtt, in this way we can know
	 * which nodes aren't already set and we can skip in node_recurse_trtt
	 * the rnodes with trtt > 0
	 */
	for(i=0; i<MAXGROUPNODE; i++)
		for(e=0; e<map[i].links; e++)
			memset(&map[i].r_node[e].trtt, 0, sizeof(struct timeval));

	for(i=0; i<MAXGROUPNODE; i++) {
		if(map[i].flags & MAP_VOID || map[i].flags & MAP_ME)
			continue;

		if(!map[i].r_node[0].trtt.tv_usec && !map[i].r_node[0].trtt.tv_sec)
			node_recurse_trtt(&map[i]);
	}
}


/*It return the node to be used as gateway to reach "node" starting from
 * root_node, using the "route"th route.
 * Used in a qspn map styleI
 */
map_node *get_gw_node(map_node *node, u_short route)
{
	map_node *ptr;
	
	if(route >= node->links || node->flags & MAP_ME)
		return NULL;
	
	ptr=(map_node *)node;
	while(1) {
		if(((map_node *)ptr->r_node[route].r_node)->flags & MAP_ME)
			break;
		ptr=(map_node *)ptr->r_node[route].r_node;
	}

	return ptr;
}

/*merge_maps: Given two maps it merge them selecting only the best routes.
 * In the "base" map there will be the resulting map. The "new" map is the
 * second map used. "base_root" points to the root_node in the "base" map.
 * "new_root" points to the root_node in the "new" map.
 * (qspn_map styleII)*/
int merge_maps(map_node *base, map_node *new, map_node *base_root, map_node *new_root)
{
	int i, e, x, count=0, rpos, nrpos;
	map_node *new_root_in_base;
	
	new_root_in_base=&base[pos_from_node(new_root, new)];
	rpos=pos_from_node(base_root, base);
		
	for(i=0; i<MAXGROUPNODE; i++) {
		if(base[i].flags & MAP_ME || new[i].flags & MAP_ME)
			continue;

		for(e=0; e<new[i].links; e++) {
			/* 
			 * Now we change the r_nodes pointers of the new map to points to 
			 * the base map's nodes. 
			 */
			nrpos=pos_from_node((map_node *)new[i].r_node[e].r_node, new);
			if(nrpos == rpos)
				/*We skip,cause the new_map it's using the base_root node as gw*/
				continue;

			if(base[nrpos].flags & MAP_VOID) {
				/*
				 * In the base we haven't the node used as gw in the new_map to reach
				 * the new[i].r_node[e] node. We must use the new_root node as gw because
				 * it is one of our rnode
				 */
				new[i].r_node[e].r_node=(int *)new_root_in_base;
			} else
				new[i].r_node[e].r_node=base[nrpos].r_node[0].r_node;
			
			if(e>=base[i].links) {
				rnode_add(&base[i], &new[i].r_node[e]);
				rnode_trtt_order(&base[i]);
				count++;
				continue;
			}
		
			/*If the worst route in base is less than the new one, let's go ahead*/
			if(get_route_trtt(&base[i], base[i].links-1, 0) < get_route_trtt(&new[i], e, 0))
				continue;
			
			for(x=0; x<base[i].links; x++) {
				if(get_route_trtt(&base[i], x, 0) > get_route_trtt(&new[i], e, 0)) {
					map_rnode_insert(&base[i], x, &new[i].r_node[e]);
					count++;
					break;
				}
			}
		}
	}
	return count;
}

/* 
 * mod_rnode_addr: Modify_rnode_address 
 */
int mod_rnode_addr(map_rnode *rnode, int *map_start, int *new_start)
{
	rnode->r_node = ((void *)rnode->r_node - (void *)map_start) + (void *)new_start;
	return 0;
}

/* 
 * get_rnode_block: It packs all the rnode structs of a node. The "r_node" pointer
 * of the map_rnode struct is changed to point to the position of the node in the map,
 * instead of the address. get_rnode_block returns the number of rnode structs packed
 */
int get_rnode_block(int *map, map_node *node, map_rnode *rblock, int rstart)
{
	int e;

	for(e=0; e<node->links; e++) {
		memcpy(&rblock[e+rstart], &node->r_node[e], sizeof(map_rnode));
		mod_rnode_addr(&rblock[e+rstart], map, 0);
	}

	return e;
}

/* 
 * map_get_rblock: It uses get_rnode_block to pack all the int_map's rnode.
 * `maxgroupnode' is the number of nodes present in the map.
 * `map' is the actual int_map, while `addr_map' is the address used by get_rnode_block
 * to change the rnodes' pointers (read get_rnode_block).
 * It returns a pointer to the start of the rnode block and stores in `count'
 * the number of rnode structs packed.
 * On error NULL is returned.
 */
map_rnode *map_get_rblock(map_node *map, int *addr_map, int maxgroupnode, int *count)
{
	int i, c=0, tot=0;
 	map_rnode *rblock;
	*count=0;
	
	for(i=0; i<maxgroupnode; i++)
		tot+=map[i].links;
	if(!tot)
		return 0;
	rblock=(map_rnode *)xmalloc(sizeof(map_rnode)*tot);

	for(i=0; i<maxgroupnode; i++)
		c+=get_rnode_block((int *)addr_map, &map[i], rblock, c);

	*count=c;	
	return rblock;
}


/* 
 * store_rnode_block: Given a correct node it restores in it all the r_node structs
 * contained in in the rnode_block. It returns the number of rnode structs restored.
 */
int store_rnode_block(int *map, map_node *node, map_rnode *rblock, int rstart) 
{
	int i;

	if(!node->links)
		return 0;

	node->r_node=xmalloc(sizeof(map_rnode)*node->links);
	for(i=0; i<node->links; i++) {
		memcpy(&node->r_node[i], &rblock[i+rstart], sizeof(map_rnode));
		mod_rnode_addr(&node->r_node[i], 0, map);
	}
	return i;
}

/* 
 * map_store_rblock: Given a correct int_map with `maxgroupnode' nodes,
 * it restores all the r_node structs in the `map' from the `rblock' 
 * using store_rnode_block. `addr_map' is the address used to change 
 * the rnodes' pointers (read store_rnode_block).
 */
int map_store_rblock(map_node *map, int *addr_map, int maxgroupnode, map_rnode *rblock)
{
	int i, c=0;
	
	for(i=0; i<maxgroupnode; i++)
		c+=store_rnode_block(addr_map, &map[i], rblock, c);
	return c;
}

int verify_int_map_hdr(struct int_map_hdr *imap_hdr, int maxgroupnode, int maxrnodeblock)
{
#ifndef QSPN_EMPIRIC /*The qspn_empiric generates a random map which has nodes
		       with map_node.links > MAXGROUPNODE;*/
	
	if(imap_hdr->rblock_sz > maxrnodeblock || 
			imap_hdr->int_map_sz > maxgroupnode*sizeof(map_node) ||
			imap_hdr->root_node > maxrnodeblock)
		return 1;
	
#endif
	return 0;
}

/* 
 * pack_map: It returns a pack of the int/bmap_map `map', which has 
 * `maxgroupnode' nodes ready to be saved or sent. In `pack_sz' it
 * stores the size of the package. For info on `addr_map' please
 * read get_map_rblock().
 */
char *pack_map(map_node *map, int *addr_map, int maxgroupnode, map_node *root_node, size_t *pack_sz)
{
	struct int_map_hdr imap_hdr;
	map_rnode *rblock;
	int count;
	char *package;

	if(!addr_map)
		addr_map=(int *)map;
	/*rblock packing*/
	rblock=map_get_rblock(map, addr_map, maxgroupnode, &count);
	/*Header creation*/
	memset(&imap_hdr, 0, sizeof(struct int_map_hdr));
	imap_hdr.root_node=root_node ? pos_from_node(root_node, map) : 0;
	imap_hdr.rblock_sz=count*sizeof(map_rnode);
	imap_hdr.int_map_sz=maxgroupnode*sizeof(map_node);

	/*Package creation*/
	*pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz);
	package=xmalloc(*pack_sz);
	memcpy(package, &imap_hdr, sizeof(struct int_map_hdr));
	memcpy(package+sizeof(struct int_map_hdr), map, imap_hdr.int_map_sz);
	if(rblock) {
		memcpy(package+sizeof(struct int_map_hdr)+imap_hdr.int_map_sz, rblock, imap_hdr.rblock_sz);
		xfree(rblock);
	}

	return package;	
}

/* 
 * unpack_map: Given a valid int/bmap_map package (packed with pack_intmap), it 
 * allocates a brand new int_map and restores in it the map and the rnodes.
 * It puts in `*new_root' the pointer to the root_node in the loaded map.
 * For info on `addr_map' please read map_store_rblock().
 * On success the a pointer to the new int_map is retuned, otherwise 0 will be
 * the fatal value.
 */
map_node *unpack_map(char *pack, size_t pack_sz, int *addr_map, map_node **new_root, 
		     int maxgroupnode, int maxrnodeblock)
{
	map_node *map;
	struct int_map_hdr *imap_hdr=(struct int_map_hdr *)pack;
	map_rnode *rblock;
	int err, nodes;
	char *p;

	if(verify_int_map_hdr(imap_hdr, maxgroupnode, maxrnodeblock)) {
		error("Malformed int/bmap_map_hdr. Aborting unpack_map().");
		return 0;
	}
		
	/*Extracting the map...*/
	p=pack+sizeof(struct int_map_hdr);
	map=init_map(0);
	if(imap_hdr->int_map_sz)
		memcpy(map, p, imap_hdr->int_map_sz);

	/*Restoring the rnodes...*/
	p+=imap_hdr->int_map_sz;
	if(imap_hdr->rblock_sz) {
		nodes=imap_hdr->int_map_sz/sizeof(map_node);
		/*Extracting the rnodes block and merging it to the map*/
		rblock=(map_rnode *)p;
		if(!addr_map)
			addr_map=(int *)map;
		err=map_store_rblock(map, addr_map, nodes, rblock);
		if(err!=imap_hdr->rblock_sz/sizeof(map_rnode)) {
			error("An error occurred while storing the rnodes block in the int/bnode_map");
			free_map(map, 0);
			return 0;
		}
	}

	if(new_root) {
		map[imap_hdr->root_node].flags|=MAP_ME;
		*new_root=&map[imap_hdr->root_node];
	}
	
	return map;
}


/* 
 * * * save/load int_map * * *
 */

int save_map(map_node *map, map_node *root_node, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the int_map in %s: %s", file, strerror(errno));
		return -1;
	}

	/*Pack!*/
	pack=pack_map(map, 0, MAXGROUPNODE, root_node, &pack_sz);
	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);
	
	xfree(pack);
	fclose(fd);
	return 0;
}

/* 
 * load_map: It loads the internal_map from `file'.
 * It returns the start of the map and if `new_root' is not NULL, it
 * puts in `*new_root' the pointer to the root_node in the loaded map.
 * On error it returns NULL. 
 */
map_node *load_map(char *file, map_node **new_root)
{
	map_node *map;
	FILE *fd;
	struct int_map_hdr imap_hdr;
	char *pack;
	size_t pack_sz;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the map from %s: %s", file, strerror(errno));
		return 0;
	}

	if(fread(&imap_hdr, sizeof(struct int_map_hdr), 1, fd) < 1)
		goto error;
	if(verify_int_map_hdr(&imap_hdr, MAXGROUPNODE, MAXRNODEBLOCK))
		goto error;
		
	rewind(fd);
	pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz);
	pack=xmalloc(pack_sz);
	if(fread(pack, pack_sz, 1, fd) < 1)
		goto error;

	map=unpack_map(pack, pack_sz, 0, new_root, MAXGROUPNODE, MAXRNODEBLOCK);
	if(!map)
		error("Cannot unpack the int_map!");

	xfree(pack);
	fclose(fd);
	return map;
error:
	error("Malformed map file. Aborting load_map().");
	return 0;
}
