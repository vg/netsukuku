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
#include "gmap.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

extern struct current me;

map_gnode *init_gmap(size_t len)
{
	map_gnode *map;
	if(!len)
		len=sizeof(map_gnode)*MAXGROUPNODE;
	
	map=(map_gnode *)xmalloc(len);
	memset(map, '\0', len);
	return map;
}

void free_gmap(map_gnode *map, size_t count)
{
	int i;
	int len=sizeof(map_gnode)*count;

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

void set_cur_gnode(u_short gid)
{
	me.cur_gid=gid;
	me.cur_gnode=GI2GMAP(me.ext_map, gid);
	me.cur_gnode->g.flags!=GMAP_ME;
}

/* gmap_get_rblock: It uses get_rnode_block to pack all the ext_map's rnode
 * It returns a pointer to the start of the rnode block and stores in "count" 
 * the number of rnode structs packed*/
map_rnode *gmap_get_rblock(map_gnode *map, int *count)
{
	int i, c=0;
 	map_rnode *rblock;
	
	for(i=0; i<MAXGROUPNODE; i++)
		tot+=map[i].g.links;
	rblock=(map_rnode *)xmalloc(sizeof(map_rnode)*tot);

	for(i=0; i<MAXGROUPNODE; i++) {
		c+=get_rnode_block((int *)map, &map[i].g, rblock, c);
	}
	
	return rblock;
}

/* gmap_store_rblock: Given a correct ext_map it restores all the r_node structs in the
 * map from the rnode_block using store_rnode_block. "count" is the number of rnode structs
 * present in the "rblock".
 */
int gmap_store_rblock(map_gnode *map, map_rnode *rblock, int count)
{
	int i, c=0;

	for(i=0; i<count; i++)
		c+=store_rnode_block((int *)map, map[i], rblock, c);

	return c; /*If it's all ok "c" has to be == sizeof(rblock)*count*/
}
