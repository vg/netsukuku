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

#include "bmap.h"
#include "xmalloc.h"
#include "log.h"

void bmap_level_init(u_char levels, map_bnode *bmap, u_int *bmap_nodes)
{
	*bmap=xmalloc(sizeof(map_bnode *) * levels);
	*bmap_nodes=xmalloc(sizeof(u_int *) * levels);
}

void bmap_level_free(map_bnode **bmap, u_int *bmap_nodes)
{
	xfree(bmap);
	xfree(bmap_nodes);
}

/* map_add_bnode: It adds a new bnode in the `bmap' and then returns its position
 * in the bmap. It also increments the `*bmap_nodes' counter. The bnode_ptr is set
 * to `bnode' and the links to `links'.
 * Note that the `bmap' argument must be an adress of a pointer.
 */
int map_add_bnode(map_bnode *bmap, u_int *bmap_nodes, u_int bnode, u_int links)
{
	map_bnode *bnode_map;
	int bm;
	
	bm=*bmap_nodes; 
	*bmap_nodes++;
	if(bmap_nodes == 1)
		*bmap=xmalloc(sizeof(map_bnode));
	else
		*bmap=xrealloc(*bmap, sizeof(map_bnode) * *bmap_nodes);

	bnode_map=*bmap;
	bnode_map[bm].bnode_ptr=bnode;
	bnode_map[bm].links=links;
	return bm;
}

/* map_bnode_del: It deletes the `bnode' in the `bmap' which has `bmap_nodes'.
 * It returns the newly rescaled `bmap'.
 * It returns 0 if the `bmap' doesn't exist anymore.*/
map_bnode *map_bnode_del(map_bnode *bmap, u_int *bmap_nodes,  map_bnode *bnode)
{
	map_node_del((map_node *)bnode);
	
	if( ((void *)bnode-(void *)bmap)/sizeof(map_bnode) != (*bmap_nodes)-1 )
		memcpy(bnode, &bmap[bmap_nodes-1], sizeof(map_bnode));

	*bmap_nodes--;
	if(*bmap_nodes)
		return xrealloc(bmap, (*bmap_nodes) * sizeof(map_bnode));
	else {
		xfree(bmap);
		return 0;
	}
}

/* map_find_bnode: Find the given `node' in the given map_bnode `bmap'.*/
int map_find_bnode(map_bnode *bmap, int count, void *void_map, void *node, u_char level)
{
	map_gnode **ext_map(map_gnode *)void_map;
	map_node  *int_map=(map_node *)void_map;
	int e;
	void *pos;

	for(e=0; e<count; e++) {
		if(!level)
			pos=(void *)node_from_pos(bmap[e].bnode_ptr, int_map);
		else
			pos=(void *)gnode_from_pos(bmap[e].bnode_ptr, ext_map[_EL(level)]);
		if(pos == node && !(bmap[e].flags & MAP_VOID))
			return e;
	}
	return -1;
}



/* * *  save/load bnode_map * * */

/* save_bmap: It saves the bnode_map `bmap' in the `file'. The `bmap' has
 * `bmap_nodes' nodes. `gmap' is the pointer to the group_node the bmap is
 * referring to.*/
int save_bmap(map_bnode *bmap, int *gmap, u_int bmap_nodes, char *file)
{
	FILE *fd;
	char *pack;
	size_t pack_sz;
	
	if(!bmap_nodes)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the bnode_map in %s: %s", file, strerror(errno));
		return -1;
	}
	
	pack=pack_map(map, gmap, bmap_nodes, 0, &pack_sz);
	fwrite(pack, pack_sz, 1, fd);

	xfree(pack);
	fclose(fd);
	return 0;
}

map_bnode *load_bmap(char *file, int *gmap, u_int *bmap_nodes)
{
	map_bnode *bmap;
	FILE *fd;
	struct bnode_map_hdr imap_hdr;
	int count, err;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the bmap from %s: %s", file, strerror(errno));
		return 0;
	}

	fread(&imap_hdr, sizeof(struct bnode_map_hdr), 1, fd);
	if(verify_int_map_hdr(&imap_hdr, MAXGROUPBNODE, MAXBNODE_RNODEBLOCK)) {
		error("Malformed bmap file: %s. Aborting load_bmap().", file);
		return 0;
	}
	*bmap_nodes=imap_hdr.bnode_map_sz/sizeof(map_bnode);

	/*Extracting the map...*/
	rewind(fd);
	pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz):
	pack=xmalloc(pack_sz);
	fread(pack, pack_sz, 1, fd);
	map=unpack_map(pack, pack_sz, gmap, 0, MAXGROUPBNODE, MAXBNODE_RNODEBLOCK);
	if(!map)
		error("Cannot unpack the bnode_map!");
	
	xfree(pack);
	fclose(fd);
	return map;
}
