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

#include <string.h>
#include "libnetlink.h"
#include "map.h"
#include "gmap.h"
#include "route.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

extern struct current me;
extern int my_family;

void ctr_add(ct_entry *ct, ct_route *ctr)
{
	list_add(ct->ctr, ctr);
}

u_char rt_find_table(ct_route *ctr, u32 dst, u32 gw)
{
	ct_route *i;
	u_char tables[253];
	
	memset(tables, '\0', 253);
	
	for(i=ctr; i; i=i->next) {
		if(i->dst==dst) {
			if(i->gw==gw)
				return i->ct_table;
			else 
				tables[i->ct_table]=1;
		}
	}

	int l;
	for(l=1; l<253; l++)
		if(!tables[l])
			return tables[l];

	return 0xff; /*This shouldn't happen!*/
}

void krnl_update_node(map_node *node)
{
	struct nexthop nh[node->links+1];
	inet_prefix to;
	int i;
	memset(nh, '\0', sizeof(nexthop), node->links+1);

	maptoip(*me.int_map, *node, me.ipstart, &to);


	/*TODO: Add gnode's support. We must use the entire gnode range as destination. can we?*/
	
	for(i=0; i<node->links; i++) {
#ifdef QMAP_STYLE_I
		maptoip(*me.int_map, *get_gw_node(node, i), me.ipstart, &nh[i].gw);
#else /*QMAP_STYLE_II*/
		maptoip(*me.int_map, *node.r_node[i].r_node, me.ipstart, &nh[i].gw);
#endif
		nh[i].dev=me.cur_dev;
		nh[i].hops=255-i;
	}

	if(node.flags & MAP_VOID) {
		/*Ok, let's delete it*/
		if(route_del(to, nh, me.cur_dev, 0))
			error("WARNING: Cannot delete the route entry for %d node!", ((void *)&node-(void *)me.int_map)/sizeof(map_node));
	}
	else
		if(route_replace(to, nh, me.cur_dev, 0))
			error("WARNING: Cannot update the route entry for %d node!", ((void *)&node-(void *)me.int_map)/sizeof(map_node));
}

void rt_update(void)
{
	u_short i;
	
	for(i=0; i<MAXGROUPNODE; i++) {
		if(me.int_map[i].flags & MAP_UPDATE && !(me.int_map[i].flags & MAP_RNODE)) {
			krnl_update_node(&me.int_map[i]);
			me.int_map[i].flags&=~MAP_UPDATE;
		}
	}
	route_flush_cache(my_family);
}

int rt_add_def_gw(char *dev) 
{
	inet_prefix to;
	
	if(inet_setip_anyaddr(&to)) {
		error("rt_add_def_gw(): Cannot use INADRR_ANY for the %d family\n", to.family);
		return -1;
	}
	return route_add(to, 0, dev, 0);
}
