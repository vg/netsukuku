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

void bmap_level_init(u_char levels, map_bnode **bmap, u_int *bmap_nodes)
{
	*bmap=xmalloc(sizeof(map_bnode *) * levels);
	*bmap_nodes=xmalloc(sizeof(u_int) * levels);
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

/* 
 * pack_all_bmaps: It creates the block of all the `bmaps' which have
 * `bmap_nodes' nodes. `ext_map' and `quadg' are the structs referring
 * to the external map. In `pack_sz' is stored the size of the block.
 * The address pointing to the block is returned otherwise 0 is given.
 */
char *
pack_all_bmaps(map_bnode **bmaps,  u_int *bmap_nodes, map_gnode **ext_map,
		quadro_group quadg, size_t *pack_sz)
{
	struct bmaps_hdr bmap_hdr;
	int i;
	size_t sz, tmp_sz[quadg.levels];
	char **pack, *final_pack, *buf;
	u_char level;

	*pack_sz=0;
	pack=xmalloc(sizeof(char *)*quadg.levels);
	
	for(level=0; level < quadg.levels; level++) {
		pack[i]=pack_map(bmaps[level], ext_map[_EL(level)], bmap_nodes[level],
				0, &sz);
		tmp_sz[level]=sz;
		*pack_sz+=sz;
	}

	bmap_hdr.levels=quadg.levels;
	bmap_hdr.bmaps_block_sz=*pack_sz;
	
	final_pack=xmalloc(*pack_sz + sizeof(struct bmaps_hdr));
	memcpy(final_pack, &bmap_hdr, sizeof(struct bmaps_hdr));
	
	buf=sizeof(struct bmaps_hdr);
	for(level=0; level < quadg.levels; level++) {
		memcpy(final_pack+buf, pack[i], tmp_sz[level]);
		buf+=(char *)tmp_sz[level];
		xfree(pack[i]);
	}

	return final_pack;
}

/*
 * unpack_all_bmaps: Given a block `pack' of size `pack_sz' containing `levels'
 * it unpacks each bnode map it finds in it. `ext_map' is the external map used
 * by the new bmaps.  In `bmap_nodes' unpack_all_bmaps stores the address of the
 * newly xmallocated array of u_int. Each bmap_nodes[x] contains the number of
 * nodes of the bmap of level x.  `maxbnodes' is the maximum number of nodes
 * each bmap can contain while `maxbnode_rnodeblock' is the maximum number of
 * rnodes each node can contain. 
 * On error 0 is returned.*/ 
map_bnode *
unpack_all_bmaps(char *pack, size_t pack_sz, u_char levels, map_gnode **ext_map,
		u_int *bmap_nodes, int maxbnodes, int maxbnode_rnodeblock)
{
	struct bnode_map_hdr *bmap_hdr;
	map_bnode **bmap;
	int i;
	char *buf;

	bmap_level_init(levels, &bmap, bmap_nodes);

	for(i=0; i<levels; i++) {
		bmap_hdr=pack+buf;
		if(verify_int_map_hdr(bmap_hdr, maxbnodes, maxbnode_rnodeblock)) {
			error("Malformed bmap_hdr at level %d. "
					"Aborting unpack_all_bmaps().", i);
			return 0;
		}
		*bmap_nodes[i]=bmap_hdr.bnode_map_sz/sizeof(map_bnode);

		/*Extracting the map...*/
		pack_sz=INT_MAP_BLOCK_SZ(bmap_hdr.bnode_map_sz, bmap_hdr.rblock_sz);
		pack=xmalloc(pack_sz);
		bmap[i]=unpack_map(pack[i], pack_sz[i], ext_map[_EL(i)], 0, 
				maxbnodes, maxbnode_rnodeblock);
		if(!bmap[i]) {
			error("Cannot unpack the bnode_map at level %d !", i);
			return 0;
		}
		buf+=pack_sz;
	}
	return bmap;
}

/* * *  save/load bnode_map * * */

/* 
 * save_bmap: It saves the bnode maps `bmaps' in `file'. The each `bmaps[x]' has
 * `bmap_nodes[x]' nodes. `ext_map' is the pointer to the external map the bmap is
 * referring to.
 */
int save_bmap(map_bnode **bmaps, u_int *bmap_nodes, map_gnode **ext_map, 
		quadro_group quadg, char *file)
{
	FILE *fd;
	char *pack;
	size_t pack_sz;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the bnode_map in %s: %s", file, strerror(errno));
		return -1;
	}
	
	pack=pack_all_bmaps(bmaps, ext_map, bmap_nodes, quadg, &pack_sz);
	fwrite(pack, pack_sz, 1, fd);

	xfree(pack);
	fclose(fd);
	return 0;
}

/*
 * load_bmap: It loads all the bnode maps from `file' and returns the address
 * of the array of pointer to the loaded bmaps. `ext_map' is the external maps
 * the bmap shall refer to. In `bmap_nodes' the address of the u_int array, used
 * to count the nodes in each bmaps, is stored. On error 0 is returned.
 */
map_bnode **load_bmap(char *file, map_gnode **ext_map, u_int *bmap_nodes)
{
	map_bnode **bmap;
	FILE *fd;
	struct bmaps_hdr bmaps_hdr;
	size_t pack_sz;
	int count, err, i;
	u_char levels;
	char *pack;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the bmap from %s: %s", file, strerror(errno));
		return 0;
	}

	
	if(fread(&bmaps_hdr, sizeof(struct bmaps_hdr), 1, fd) < 1 ) {
		error("Cannot load the bmaps_hdr from %s", file);
		return 0;
	}
	levels=bmaps_hdr.levels;
	pack_sz=bmaps_hdr.bmaps_block_sz;
	if(levels > GET_LEVELS(my_family) || pack_sz < sizeof(struct bnode_map_hdr)) {
		error("Malformed bmaps_hdr. Cannot load the bnode maps.");
		return 0;
	}

	/* Extracting the map... */
	if(fread(pack, bmaps_hdr.bmaps_block_sz, 1, fd) < 1)
		goto error;
	bmap=unpack_all_bmaps(pack, pack_sz, levels, ext_map, bmap_nodes, 
			MAXGROUPNODE, MAXBNODE_RNODEBLOCK);
	xfree(pack);
	fclose(fd);
	return bmap;
}
